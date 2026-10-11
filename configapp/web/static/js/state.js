class AppState {
    constructor() {
        this.initialConfig = null;
        this.currentConfig = null;
        this.currentEtag = null;
        this.serviceStatus = { is_active: false, status_text: 'Unknown' };
        this.listeners = [];
    }

    subscribe(listener) {
        this.listeners.push(listener);
    }

    notify() {
        this.listeners.forEach(l => l(this));
    }

    setConfig(config, etag) {
        this.initialConfig = JSON.parse(JSON.stringify(config)); // Deep clone for snapshot
        this.currentConfig = config;
        this.currentEtag = etag;
        this.notify();
    }

    updateCurrentConfig(path, value) {
        // Упрощенное обновление по пути (например, "defaults.logging.level")
        const keys = path.split('.');
        let obj = this.currentConfig;
        for (let i = 0; i < keys.length - 1; i++) {
            obj = obj[keys[i]];
        }
        obj[keys[keys.length - 1]] = value;
        this.notify();
    }

    resetToInitial() {
        this.currentConfig = JSON.parse(JSON.stringify(this.initialConfig));
        this.notify();
    }

    setServiceStatus(status) {
        this.serviceStatus = status;
        this.notify();
    }
}

export const state = new AppState();