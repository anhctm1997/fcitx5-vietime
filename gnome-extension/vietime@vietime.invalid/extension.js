import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const IFACE = `
<node><interface name="org.vietime.FocusTracker">
  <method name="GetFocusedApplication"><arg type="a{ss}" direction="out"/></method>
  <method name="ListRunningApplications"><arg type="aa{ss}" direction="out"/></method>
  <method name="ListRecentApplications"><arg type="aa{ss}" direction="out"/></method>
  <signal name="FocusChanged"><arg type="a{ss}"/></signal>
</interface></node>`;

export default class VietIMEFocusTracker extends Extension {
    _identity(win) {
        if (!win)
            return {};
        const tracker = Shell.WindowTracker.get_default();
        const app = tracker.get_window_app(win);
        const desktopId = app?.get_id() ?? '';
        const wmClass = win.get_wm_class() ?? win.get_wm_class_instance() ?? '';
        return {
            name: app?.get_name() ?? wmClass,
            desktopId,
            appId: desktopId.replace(/\.desktop$/i, ''),
            wmClass,
            executable: app?.get_app_info()?.get_executable() ?? '',
            icon: app?.get_app_info()?.get_icon()?.to_string() ?? '',
        };
    }

    GetFocusedApplication() {
        return this._identity(global.display.focus_window);
    }

    ListRunningApplications() {
        const seen = new Set();
        const result = [];
        for (const actor of global.get_window_actors()) {
            const identity = this._identity(actor.meta_window);
            const key = identity.desktopId || identity.wmClass;
            if (key && !seen.has(key)) {
                seen.add(key);
                result.push(identity);
            }
        }
        return result;
    }

    ListRecentApplications() {
        return [...this._recent.values()];
    }

    _remember(identity) {
        const key = identity.desktopId || identity.appId || identity.wmClass;
        if (!key)
            return;
        this._recent.delete(key);
        this._recent.set(key, identity);
        while (this._recent.size > 30)
            this._recent.delete(this._recent.keys().next().value);
    }

    enable() {
        this._recent = new Map();
        for (const identity of this.ListRunningApplications())
            this._remember(identity);
        this._dbus = Gio.DBusExportedObject.wrapJSObject(IFACE, this);
        this._dbus.export(Gio.DBus.session, '/org/vietime/FocusTracker');
        this._name = Gio.bus_own_name_on_connection(
            Gio.DBus.session, 'org.vietime.FocusTracker', Gio.BusNameOwnerFlags.NONE,
            null, null);
        this._focusSignal = global.display.connect('notify::focus-window', () => {
            const identity = this._identity(global.display.focus_window);
            this._remember(identity);
            this._dbus.emit_signal('FocusChanged',
                new GLib.Variant('(a{ss})', [identity]));
        });
    }

    disable() {
        if (this._focusSignal)
            global.display.disconnect(this._focusSignal);
        if (this._name)
            Gio.bus_unown_name(this._name);
        this._dbus?.unexport();
        this._dbus = null;
        this._recent = null;
    }
}
