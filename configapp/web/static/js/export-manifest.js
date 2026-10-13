import { exportApi } from './export-api.js';

export class ManifestManager {
    constructor() {
        this.container = document.getElementById('manifest-container');
        document.getElementById('btn-refresh-manifest').addEventListener('click', () => this.loadManifest());
    }

    async loadManifest() {
        this.container.innerHTML = '<p>Loading manifest...</p>';
        try {
            const data = await exportApi.getManifest();
            this._renderTable(data.files);
        } catch (e) {
            this.container.innerHTML = `<p class="error-msg">Failed to load manifest: ${e.message}</p>`;
        }
    }

    _renderTable(files) {
        if (!files || files.length === 0) {
            this.container.innerHTML = '<p>No excluded files found in monitored directories.</p>';
            return;
        }

        let html = `<table>
            <thead><tr><th>Source</th><th>File Path</th><th>Size</th><th>Hash</th><th>Actions</th></tr></thead>
            <tbody>`;
        
        for (const f of files) {
            const sizeKb = (f.size_bytes / 1024).toFixed(2);
            const downloadUrl = `/api/v1/export/download?token=${encodeURIComponent(f.download_token)}`;
            
            html += `<tr>
                <td>${f.source_name}</td>
                <td><code>${f.file_path}</code></td>
                <td>${sizeKb} KB</td>
                <td><code title="${f.file_hash}">${f.file_hash.substring(0, 8)}...</code></td>
                <td>
                    <a href="${downloadUrl}" target="_blank" class="btn-primary" style="text-decoration:none; display:inline-block; margin-right:5px;">Download</a>
                    <button class="btn-warning" onclick="window.manifestManager.acknowledge('${f.ack_token}')">ACK (Delete/Move)</button>
                </td>
            </tr>`;
        }
        html += '</tbody></table>';
        this.container.innerHTML = html;
    }

    async acknowledge(token) {
        if (!confirm('Confirm receipt? The file will be deleted or moved according to the profile.')) return;
        try {
            await exportApi.acknowledge(token);
            this.loadManifest(); // Refresh list
        } catch (e) {
            alert('ACK failed: ' + e.message);
        }
    }
}