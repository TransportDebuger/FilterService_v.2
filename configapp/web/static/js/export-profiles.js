import { exportApi } from './export-api.js';

export class ProfilesManager {
    constructor() {
        this.container = document.getElementById('profiles-container');
        this.formContainer = document.getElementById('profile-form-container');
        this.form = document.getElementById('profile-form');
        this._bindEvents();
    }

    _bindEvents() {
        document.getElementById('btn-add-profile').addEventListener('click', () => this._showForm());
        document.getElementById('btn-cancel-profile').addEventListener('click', () => this._hideForm());
        document.getElementById('profile-action').addEventListener('change', (e) => {
            document.getElementById('move-config').classList.toggle('hidden', e.target.value !== 'move');
        });
        document.getElementById('profile-transport').addEventListener('change', (e) => this._renderTransportFields(e.target.value));
        
        this.form.addEventListener('submit', async (e) => {
            e.preventDefault();
            await this._saveProfile();
        });
    }

    async loadProfiles() {
        try {
            const profiles = await exportApi.getProfiles();
            this._renderTable(profiles);
        } catch (e) {
            console.error(e);
        }
    }

    _renderTable(profiles) {
        if (profiles.length === 0) {
            this.container.innerHTML = '<p>No transport profiles configured.</p>';
            return;
        }

        let html = `<table>
            <thead><tr><th>Source</th><th>Transport</th><th>ACK Action</th><th>Config</th><th>Actions</th></tr></thead>
            <tbody>`;
        
        for (const p of profiles) {
            const transportBadge = p.transport_type === 'pull' ? 'badge-pull' : 'badge-push';
            const actionBadge = p.ack_action === 'delete' ? 'badge-delete' : 'badge-move';
            
            html += `<tr>
                <td>${p.source_name}</td>
                <td><span class="badge ${transportBadge}">${p.transport_type.toUpperCase()}</span></td>
                <td><span class="badge ${actionBadge}">${p.ack_action.toUpperCase()}</span></td>
                <td><code>${JSON.stringify(p.transport_config)}</code></td>
                <td>
                    <button class="btn-danger" onclick="window.profilesManager.deleteProfile(${p.id})">Delete</button>
                </td>
            </tr>`;
        }
        html += '</tbody></table>';
        this.container.innerHTML = html;
    }

    _showForm(profile = null) {
        this.formContainer.classList.remove('hidden');
        document.getElementById('profile-id').value = profile ? profile.id : '';
        document.getElementById('profile-source').value = profile ? profile.source_name : '';
        document.getElementById('profile-transport').value = profile ? profile.transport_type : 'pull';
        document.getElementById('profile-action').value = profile ? profile.ack_action : 'delete';
        
        this._renderTransportFields(profile ? profile.transport_type : 'pull', profile ? profile.transport_config : {});
        
        if (profile && profile.ack_action === 'move') {
            document.getElementById('move-config').classList.remove('hidden');
            document.getElementById('profile-archive-dir').value = profile.transport_config.archive_dir || '';
        } else {
            document.getElementById('move-config').classList.add('hidden');
        }
    }

    _hideForm() {
        this.formContainer.classList.add('hidden');
        this.form.reset();
    }

    _renderTransportFields(type, config = {}) {
        const container = document.getElementById('transport-config-fields');
        container.innerHTML = '';

        if (type === 'webhook') {
            container.innerHTML = `
                <div class="field-row">
                    <label>Webhook URL:</label>
                    <input type="text" id="cfg-url" value="${config.url || ''}" required>
                </div>`;
        } else if (type === 'rabbitmq') {
            container.innerHTML = `
                <div class="field-row">
                    <label>Broker URL:</label>
                    <input type="text" id="cfg-broker" value="${config.broker_url || ''}" placeholder="amqp://guest:guest@localhost:5672/" required>
                </div>
                <div class="field-row">
                    <label>Queue Name:</label>
                    <input type="text" id="cfg-queue" value="${config.queue_name || ''}" required>
                </div>`;
        }
    }

    async _saveProfile() {
        const id = document.getElementById('profile-id').value;
        const source = document.getElementById('profile-source').value;
        const transport = document.getElementById('profile-transport').value;
        const action = document.getElementById('profile-action').value;
        
        let transportConfig = {};
        if (transport === 'webhook') {
            transportConfig.url = document.getElementById('cfg-url').value;
        } else if (transport === 'rabbitmq') {
            transportConfig.broker_url = document.getElementById('cfg-broker').value;
            transportConfig.queue_name = document.getElementById('cfg-queue').value;
        }
        
        if (action === 'move') {
            transportConfig.archive_dir = document.getElementById('profile-archive-dir').value;
        }

        const payload = {
            id: id ? parseInt(id) : null,
            source_name: source,
            transport_type: transport,
            transport_config: transportConfig,
            ack_action: action
        };

        try {
            await exportApi.saveProfile(payload);
            this._hideForm();
            this.loadProfiles();
        } catch (e) {
            alert('Error saving profile: ' + e.message);
        }
    }

    async deleteProfile(id) {
        if (!confirm('Delete this profile?')) return;
        try {
            await exportApi.deleteProfile(id);
            this.loadProfiles();
        } catch (e) {
            alert('Error deleting profile: ' + e.message);
        }
    }
}