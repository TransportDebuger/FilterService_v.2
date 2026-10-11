// Генерация JSON Patch (RFC 6902)
export function generatePatch(original, modified, basePath = '') {
    const patch = [];

    function compare(obj1, obj2, path) {
        if (obj1 === obj2) return;

        if (typeof obj1 !== 'object' || obj1 === null || typeof obj2 !== 'object' || obj2 === null) {
            if (obj1 !== obj2) {
                patch.push({ op: 'replace', path: path, value: obj2 });
            }
            return;
        }

        if (Array.isArray(obj1) && Array.isArray(obj2)) {
            // Упрощенная обработка массивов: пересоздание целиком при изменении
            // Для продакшена требуется более сложный алгоритм с diff'ом элементов
            if (JSON.stringify(obj1) !== JSON.stringify(obj2)) {
                patch.push({ op: 'replace', path: path, value: obj2 });
            }
            return;
        }

        const keys1 = Object.keys(obj1);
        const keys2 = Object.keys(obj2);

        // Проверка удаленных ключей
        for (const key of keys1) {
            if (!keys2.includes(key)) {
                patch.push({ op: 'remove', path: `${path}/${escapeJsonPointer(key)}` });
            }
        }

        // Проверка добавленных и измененных ключей
        for (const key of keys2) {
            const newPath = `${path}/${escapeJsonPointer(key)}`;
            if (!keys1.includes(key)) {
                patch.push({ op: 'add', path: newPath, value: obj2[key] });
            } else {
                compare(obj1[key], obj2[key], newPath);
            }
        }
    }

    compare(original, modified, basePath);
    return patch;
}

function escapeJsonPointer(str) {
    return String(str).replace(/~/g, '~0').replace(/\//g, '~1');
}