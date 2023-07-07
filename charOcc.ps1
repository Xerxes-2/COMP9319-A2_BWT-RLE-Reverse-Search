#!/usr/bin/pwsh
param([string]$FilePath, [char]$Char, [int]$Pos)

if (!(Test-Path $FilePath)) {
    Write-Host "File $FilePath does not exist."
    exit
}

$fileContent = Get-Content -Path $FilePath -Raw

$charCount = 0


for ($i = 0; $i -lt $Pos; $i++) {
    if ($fileContent[$i] -ceq $Char) {
        $charCount++
    }
}

Write-Host "$charCount"
