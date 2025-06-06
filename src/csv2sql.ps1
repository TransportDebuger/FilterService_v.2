# Параметры
$csvFile = "data.csv"         # Имя вашего CSV-файла
$sqlFile = "insert.sql"       # Имя выходного SQL-файла
$table   = "your_table"       # Имя таблицы

# Чтение CSV с кодировкой Windows-1251
$csv = Import-Csv -Path $csvFile -Delimiter ',' -Encoding Default

# Открытие файла для записи
$writer = [System.IO.StreamWriter]::new($sqlFile, $false, [System.Text.Encoding]::UTF8)

foreach ($row in $csv) {
    # Получаем имена столбцов (первые 6)
    $columns = $row.PSObject.Properties.Name | Select-Object -First 6
    # Получаем значения (первые 6)
    $values = @()
    foreach ($col in $columns) {
        $val = $row.$col
        if ([string]::IsNullOrWhiteSpace($val)) {
            $values += "NULL"
        } else {
            $escaped = $val -replace "'", "''"
            $values += "'$escaped'"
        }
    }
    $colsStr = ($columns -join ", ")
    $valsStr = ($values -join ", ")
    $writer.WriteLine("INSERT INTO $table ($colsStr) VALUES ($valsStr);")
}

$writer.Close()
Write-Host "Готово! SQL-запросы записаны в $sqlFile"