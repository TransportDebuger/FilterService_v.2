import { state } from './state.js';

// Упрощенный рендерер, адаптированный под структуру config.json
export class SchemaRenderer {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
    }

    render(config) {
        this.container.innerHTML = '';
        this.container.appendChild(this._renderObject(config, '', 'root'));
    }

    _renderObject(obj, path, title) {
        const group = document.createElement('div');
        group.className = 'field-group';
        
        const header = document.createElement('h3');
        header.textContent = title || 'Configuration';
        group.appendChild(header);

        for (const [key, value] of Object.entries(obj)) {
            const currentPath = path ? `${path}.${key}` : key;
            
            if (typeof value === 'object' && value !== null && !Array.isArray(value)) {
                // Рекурсия для вложенных объектов
                const subGroup = this._renderObject(value, currentPath, key);
                group.appendChild(subGroup);
            } else if (Array.isArray(value)) {
                group.appendChild(this._renderArray(value, currentPath, key));
            } else {
                group.appendChild(this._renderPrimitive(value, currentPath, key));
            }
        }
        return group;
    }

    _renderArray(arr, path, title) {
        const group = document.createElement('div');
        group.className = 'field-group';
        
        const header = document.createElement('h3');
        header.textContent = `${title} (Array)`;
        group.appendChild(header);

        const listContainer = document.createElement('div');
        
        arr.forEach((item, index) => {
            const itemPath = `${path}.${index}`;
            const itemDiv = document.createElement('div');
            itemDiv.className = 'array-item';
            
            if (typeof item === 'object' && item !== null) {
                itemDiv.appendChild(this._renderObject(item, itemPath, `Item ${index}`));
            } else {
                itemDiv.appendChild(this._renderPrimitive(item, itemPath, `Item ${index}`));
            }

            const delBtn = document.createElement('button');
            delBtn.className = 'btn-danger';
            delBtn.textContent = 'Delete';
            delBtn.onclick = () => {
                // Удаление элемента из массива в state
                const currentArr = this._getValueByPath(state.currentConfig, path);
                currentArr.splice(index, 1);
                state.notify();
            };
            itemDiv.appendChild(delBtn);
            listContainer.appendChild(itemDiv);
        });

        group.appendChild(listContainer);

        const addBtn = document.createElement('button');
        addBtn.className = 'btn-add';
        addBtn.textContent = `Add ${title}`;
        addBtn.onclick = () => {
            const currentArr = this._getValueByPath(state.currentConfig, path);
            // Добавление пустого объекта или строки в зависимости от контекста
            currentArr.push({}); 
            state.notify();
        };
        group.appendChild(addBtn);

        return group;
    }

    _renderPrimitive(value, path, key) {
        const row = document.createElement('div');
        row.className = 'field-row';

        const label = document.createElement('label');
        label.textContent = key;
        row.appendChild(label);

        let input;
        if (typeof value === 'boolean') {
            input = document.createElement('input');
            input.type = 'checkbox';
            input.checked = value;
            input.onchange = () => state.updateCurrentConfig(path, input.checked);
        } else if (typeof value === 'number') {
            input = document.createElement('input');
            input.type = 'number';
            input.value = value;
            input.onchange = () => state.updateCurrentConfig(path, Number(input.value));
        } else {
            input = document.createElement('input');
            input.type = 'text';
            input.value = value;
            input.onchange = () => state.updateCurrentConfig(path, input.value);
        }
        
        row.appendChild(input);
        return row;
    }

    _getValueByPath(obj, path) {
        return path.split('.').reduce((acc, part) => acc[part], obj);
    }
}