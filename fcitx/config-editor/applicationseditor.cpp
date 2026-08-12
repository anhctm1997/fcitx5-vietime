#include "applicationseditor.h"
#include <algorithm>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

namespace {
using ApplicationIdentity = QMap<QString, QString>;
using ApplicationList = QList<ApplicationIdentity>;

QString identityValue(const ApplicationIdentity &identity) {
    for (const auto &key : {"appId", "desktopId", "wmClass", "executable"}) {
        const auto value = identity.value(key);
        if (!value.isEmpty())
            return value;
    }
    return {};
}

QStringList normalizeAliases(const QStringList &input) {
    QStringList aliases;
    for (auto value : input) {
        value = value.trimmed().toLower();
        if (!value.isEmpty() && !aliases.contains(value))
            aliases.push_back(value);
    }
    const auto current = aliases;
    for (const auto &alias : current) {
        const auto basename = QFileInfo(alias).fileName();
        if (!basename.isEmpty() && !aliases.contains(basename))
            aliases.push_back(basename);
        if (alias.endsWith(".desktop")) {
            const auto withoutSuffix = alias.first(alias.size() - 8);
            if (!aliases.contains(withoutSuffix))
                aliases.push_back(withoutSuffix);
        }
    }
    return aliases;
}

QStringList identityAliases(const ApplicationIdentity &identity) {
    QStringList aliases;
    for (const auto &key : {"appId", "desktopId", "wmClass", "executable"})
        aliases.push_back(identity.value(key));
    return normalizeAliases(aliases);
}

QString cleanIniValue(QString value) {
    return value.replace('\n', ' ').replace('\r', ' ').trimmed();
}

ApplicationList trackerApplications(const char *method, QString *error) {
    QDBusInterface tracker("org.vietime.FocusTracker",
                           "/org/vietime/FocusTracker",
                           "org.vietime.FocusTracker",
                           QDBusConnection::sessionBus());
    if (!tracker.isValid()) {
        *error = QObject::tr("GNOME focus tracker is not enabled. Enable "
                            "vietime@vietime.invalid and reopen this dialog.");
        return {};
    }
    const QDBusMessage reply = tracker.call(method);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        *error = reply.errorMessage();
        return {};
    }
    const auto argument = qvariant_cast<QDBusArgument>(reply.arguments().first());
    ApplicationList result;
    argument.beginArray();
    while (!argument.atEnd()) {
        ApplicationIdentity identity;
        argument >> identity;
        result.push_back(std::move(identity));
    }
    argument.endArray();
    return result;
}
} // namespace

VietimeApplicationsEditor::VietimeApplicationsEditor(QWidget *parent)
    : FcitxQtConfigUIWidget(parent), rules_(new QListWidget(this)),
      remove_(new QPushButton(tr("Remove"), this)),
      status_(new QLabel(this)) {
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("Applications below use anti-flicker preedit. Rules are matched "
           "case-insensitively and support * and ?."), this));
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status_);
    layout->addWidget(rules_);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add…"), this);
    buttons->addWidget(add);
    buttons->addWidget(remove_);
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this,
            &VietimeApplicationsEditor::addApplication);
    connect(remove_, &QPushButton::clicked, this, [this] {
        if (rules_->currentRow() < 0)
            return;
        delete rules_->takeItem(rules_->currentRow());
        if (!saveImmediately())
            load();
    });
    connect(rules_, &QListWidget::itemSelectionChanged, this,
            [this] { remove_->setEnabled(rules_->currentRow() >= 0); });
    remove_->setEnabled(false);
    load();
    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this,
            &VietimeApplicationsEditor::refreshStatus);
    timer->start();
}

QString VietimeApplicationsEditor::configPath() const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath("fcitx5/conf/vietime-applications.conf");
}

QString VietimeApplicationsEditor::statusPath() const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath("vietime/status.conf");
}

void VietimeApplicationsEditor::load() {
    rules_->clear();
    QSettings settings(configPath(), QSettings::IniFormat);
    settings.beginGroup("Rules");
    auto keys = settings.childKeys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return a.toInt() < b.toInt();
    });
    for (const auto &key : keys) {
        const auto aliases = settings.value(key).toString().split(
            '|', Qt::SkipEmptyParts);
        settings.endGroup();
        settings.beginGroup("Names");
        const auto name = settings.value(key, aliases.value(0)).toString();
        settings.endGroup();
        settings.beginGroup("Sources");
        const auto source = settings.value(key, "manual").toString();
        settings.endGroup();
        settings.beginGroup("Rules");
        auto *item = new QListWidgetItem(name, rules_);
        item->setData(Qt::UserRole, aliases);
        item->setData(Qt::UserRole + 1, source);
        item->setToolTip(aliases.join(", "));
    }
    settings.endGroup();
    refreshStatus();
    Q_EMIT changed(false);
}

void VietimeApplicationsEditor::save() {
    saveImmediately();
}

bool VietimeApplicationsEditor::saveImmediately() {
    QDir().mkpath(QFileInfo(configPath()).absolutePath());
    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Cannot save application rules"),
                              file.errorString());
        return false;
    }
    QTextStream output(&file);
    output << "[Rules]\n";
    for (int i = 0; i < rules_->count(); ++i)
        output << i << '=' << rules_->item(i)->data(Qt::UserRole)
                                  .toStringList().join('|') << '\n';
    output << "\n[Names]\n";
    for (int i = 0; i < rules_->count(); ++i)
        output << i << '=' << cleanIniValue(rules_->item(i)->text()) << '\n';
    output << "\n[Sources]\n";
    for (int i = 0; i < rules_->count(); ++i)
        output << i << '=' << cleanIniValue(
            rules_->item(i)->data(Qt::UserRole + 1).toString()) << '\n';
    if (!file.commit()) {
        QMessageBox::critical(this, tr("Cannot save application rules"),
                              file.errorString());
        return false;
    }
    QProcess::startDetached("fcitx5-remote", {"-r"});
    refreshStatus();
    Q_EMIT changed(false);
    return true;
}

void VietimeApplicationsEditor::refreshStatus() {
    const auto bus = QDBusConnection::sessionBus().interface();
    const bool trackerActive = bus &&
        bus->isServiceRegistered("org.vietime.FocusTracker");
    QSettings runtime(statusPath(), QSettings::IniFormat);
    const auto mode = runtime.value("Mode", tr("not loaded yet")).toString();
    const auto matched = runtime.value("MatchedRule").toString();
    const auto program = runtime.value("Program").toString();
    QStringList identities;
    runtime.beginGroup("Identities");
    for (const auto &key : runtime.childKeys())
        identities.push_back(runtime.value(key).toString());
    runtime.endGroup();
    const auto backend = trackerActive ? tr("active") : tr("unavailable");
    status_->setText(tr("Focus tracker: %1 | Last engine mode: %2 | Matched rule: %3\n"
                        "Fcitx program: %4 | Identities: %5")
                         .arg(backend, mode,
                              matched.isEmpty() ? tr("none") : matched,
                              program.isEmpty() ? tr("unknown") : program,
                              identities.isEmpty() ? tr("none")
                                                   : identities.join(", ")));
}

void VietimeApplicationsEditor::addApplication() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add application"));
    dialog.resize(620, 440);
    auto *layout = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget(&dialog);
    auto *open = new QListWidget(tabs);
    auto *recent = new QListWidget(tabs);
    auto *manual = new QWidget(tabs);
    auto *manualLayout = new QFormLayout(manual);
    auto *manualValue = new QLineEdit(manual);
    manualValue->setPlaceholderText(tr("program, application ID, or wildcard"));
    manualLayout->addRow(tr("Identity:"), manualValue);
    tabs->addTab(open, tr("Open applications"));
    tabs->addTab(recent, tr("Recent"));
    tabs->addTab(manual, tr("Enter manually"));
    layout->addWidget(tabs);

    QString error;
    for (const auto &entry : trackerApplications("ListRunningApplications", &error)) {
        const auto &identity = entry;
        const auto value = identityValue(identity);
        if (value.isEmpty())
            continue;
        auto *item = new QListWidgetItem(
            QString("%1\n%2").arg(identity.value("name"), value), open);
        const auto aliases = identityAliases(identity);
        item->setData(Qt::UserRole, aliases);
        item->setToolTip(tr("Verified identities: %1").arg(aliases.join(", ")));
    }
    if (!error.isEmpty()) {
        auto *notice = new QLabel(error, &dialog);
        notice->setWordWrap(true);
        layout->insertWidget(0, notice);
    }
    QString recentError;
    for (const auto &entry : trackerApplications("ListRecentApplications", &recentError)) {
        const auto &identity = entry;
        const auto value = identityValue(identity);
        if (value.isEmpty())
            continue;
        auto *item = new QListWidgetItem(
            QString("%1\n%2").arg(identity.value("name"), value),
            recent);
        item->setData(Qt::UserRole, identityAliases(identity));
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                         QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QStringList values;
    QString name;
    QString source;
    if (tabs->currentWidget() == open && open->currentItem()) {
        values = open->currentItem()->data(Qt::UserRole).toStringList();
        name = open->currentItem()->text().section('\n', 0, 0);
        source = "gnome";
    } else if (tabs->currentWidget() == recent && recent->currentItem()) {
        values = recent->currentItem()->data(Qt::UserRole).toStringList();
        name = recent->currentItem()->text().section('\n', 0, 0);
        source = "gnome";
    } else if (tabs->currentWidget() == manual) {
        values = {manualValue->text()};
        name = manualValue->text();
        source = "manual";
    }
    values = normalizeAliases(values);
    values.removeAll("");
    if (values.isEmpty())
        return;
    for (int i = 0; i < rules_->count(); ++i) {
        const auto existing = rules_->item(i)->data(Qt::UserRole).toStringList();
        for (const auto &value : values) {
            if (existing.contains(value))
                return;
        }
    }
    auto *item = new QListWidgetItem(name.trimmed(), rules_);
    item->setData(Qt::UserRole, values);
    item->setData(Qt::UserRole + 1, source);
    item->setToolTip(values.join(", "));
    if (!saveImmediately())
        load();
}

QString VietimeApplicationsEditor::title() {
    return tr("VietIME application rules");
}
QString VietimeApplicationsEditor::icon() { return "input-keyboard"; }
