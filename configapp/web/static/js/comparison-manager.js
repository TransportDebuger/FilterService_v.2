import { comparisonApi } from './comparison-api.js';

export class ComparisonManager {
    constructor() {
        this.listSelect = document.getElementById('comparison-list-select');
        this.recordsContainer = document.getElementById('comparison-records-container');
        this.formContainer = document.getElementById('record-form-container');
        this.form = document.getElementById('record-form');
        this.versionBadge = document.getElementById('current-version');
        
        this.currentListId = null;
        this.currentVersion = 0;

        this._bindEvents();
    }

    _bindEvents() {
        document.getElementById('btn-refresh-lists').addEventListener('click', () => this.loadLists());
        this.listSelect.addEventListener('change', (e) => this._onListSelected(e.target.value));
        
        document.getElementById('btn-cancel-record').addEventListener('click', () => this._hideForm());
        this.form.addEventListener('submit', async (e) => {
            e.preventDefault();
            await this._saveRecord();
        });
    }

    async loadLists() {
        try {
            const lists = await comparisonApi.getLists();
            this.listSelect.innerHTML = '<option value="">-- Select List --</option>';
            lists.forEach(l => {
                const opt = document.createElement('option');
                opt.value = l.id;
                opt.textContent = `${l.db_path} (v${l.serial_version})`;
                this.listSelect.appendChild(opt);
            });
        } catch (e) {
            console.error(e);
        }
    }

    async _onListSelected(listId) {
        if (!listId) {
            this.recordsContainer.innerHTML = '';
            this.versionBadge.textContent = '';
            return;
        }
        this.currentListId = parseInt(listId);
        await this._loadRecords();
    }

    async _loadRecords() {
        this.recordsContainer.innerHTML = '<p>Loading...</p>';
        try {
            const [records, versionData] = await Promise.all([
                comparisonApi.getRecords(this.currentListId),
                comparisonApi.getVersion(this.currentListId)
            ]);
            
            this.currentVersion = versionData.serial_version;
            this.versionBadge.textContent = `Version: ${this.currentVersion}`;
            this._renderRecords(records);
        } catch (e) {
            this.recordsContainer.innerHTML = `<p class="error-msg">${e.message}</p>`;
        }
    }

    _renderRecords(records) {
        if (records.length === 0) {
            this.recordsContainer.innerHTML = '<p>No records found. <button class="btn-primary" onclick="window.comparisonManager.showAddForm()">Add First Record</button></p>';
            return;
        }

        let html = `<table>
            <thead><tr><th>ID</th><th>Data (JSON)</th><th>Version</th><th>Actions</th></tr></thead>
            <tbody>`;
        
        for (const r of records) {
            const dataStr = JSON.stringify(r.data, null, 2);
            const preview = dataStr.length > 50 ? dataStr.substring(0, 50) + '...' : dataStr;
            
            html += `<tr>
                <td>${r.id}</td>
                <td><code title="${this._escapeHtml(dataStr)}">${this._escapeHtml(preview)}</code></td>
                <td>${r.record_version}</td>
                <td>
                    <button class="btn-warning" onclick="window.comparisonManager.showEditForm(${r.id}, '${this._escapeHtml(dataStr).replace(/'/g, "\\'")}')">Edit</button>
                    <button class="btn-danger" onclick="window.comparisonManager.deleteRecord(${r.id})">Delete</button>
                </td>
            </tr>`;
        }
        html += '</tbody></table>';
        html += '<button class="btn-primary" style="margin-top:10px;" onclick="window.comparisonManager.showAddForm()">Add Record</button>';
        this.recordsContainer.innerHTML = html;
    }

    showAddForm() {
        document.getElementById('record-form-title').textContent = 'New Record';
        document.getElementById('record-id').value = '';
        document.getElementById('record-data').value = '{}';
        this.formContainer.classList.remove('hidden');
    }

    showEditForm(recordId, dataStr) {
        document.getElementById('record-form-title').textContent = `Edit Record #${recordId}`;
        document.getElementById('record-id').value = recordId;
        document.getElementById('record-data').value = dataStr;
        this.formContainer.classList.remove('hidden');
    }

    _hideForm() {
        this.formContainer.classList.add('hidden');
        this.form.reset();
    }

    async _saveRecord() {
        const recordId = document.getElementById('record-id').value;
        const dataStr = document.getElementById('record-data').value;
        
        let dataObj;
        try {
            dataObj = JSON.parse(dataStr);
        } catch (e) {
            alert('Invalid JSON format in Data field');
            return;
        }

        const operations = [];
        if (recordId) {
            operations.push({ op: 'update', record_id: parseInt(recordId), data: dataObj });
        } else {
            operations.push({ op: 'add', data: dataObj });
        }

        try {
            const result = await comparisonApi.applyBatch(this.currentListId, this.currentVersion, operations);
            alert(`Success! New version: ${result.new_version}. ${result.message || ''}`);
            this._hideForm();
            await this._loadRecords(); // Refresh table and version badge
        } catch (e) {
            alert('Error: ' + e.message);
        }
    }

    async deleteRecord(recordId) {
        if (!confirm('Delete this record?')) return;
        
        const operations = [{ op: 'delete', record_id: recordId }];
        try {
            await comparisonApi.applyBatch(this.currentListId, this.currentVersion, operations);
            await this._loadRecords();
        } catch (e) {
            alert('Error: ' + e.message);
        }
    }

    _escapeHtml(text) {
        const map = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#039;' };
        return text.replace(/[&<>"']/g, m => map[m]);
    }
}