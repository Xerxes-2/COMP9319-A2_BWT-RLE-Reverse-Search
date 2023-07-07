#!/usr/bin/pwsh
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$FilePath
)

if (!(Test-Path $FilePath)) {
    Write-Host "File $FilePath does not exist."
    exit
}

$fileContent = Get-Content -Path $FilePath -Raw

$charCount = @{}
$charCount[9] = 0
$charCount[10] = 0
$charCount[13] = 0

for ($i = 32; $i -le 126; $i++) {
    $charCount[$i] = 0
}

foreach ($char in [char[]]$fileContent) {
    $ascii = [int][char]$char
    if (($ascii -ge 9 -and $ascii -le 10) -or $ascii -eq 13 -or ($ascii -ge 32 -and $ascii -le 126)) {
        if ($charCount.ContainsKey($ascii)) {
            $charCount[$ascii]++
        }
    }
}

$charCount.GetEnumerator() | Sort-Object { $_.Key } | ForEach-Object { "$([char]$_.Key): $($_.Value)" }
