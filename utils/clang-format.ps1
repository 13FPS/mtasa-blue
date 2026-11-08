#!/usr/bin/env pwsh

[CmdletBinding()]
param(
    # Relative selections are rooted at the repository; omitting -Files keeps
    # the existing whole-source-tree formatting and Linux CI pass.
    [ValidateNotNullOrEmpty()]
    [string[]]$Files
)

# Check PowerShell version
if ($PSVersionTable.PSVersion.Major -lt 7) {
    $currentVersion = $PSVersionTable.PSVersion
    Write-Error @"
This script requires PowerShell 7 or later.
Current version: $currentVersion

Install PowerShell 7+:
  - Or run: winget install Microsoft.PowerShell (Windows)
  - Windows/macOS/Linux: https://github.com/PowerShell/PowerShell#get-powershell
"@
    exit 1
}

$ToolConfig = @{
    "clang-format" = @{
        "linux-amd64" = @{
            "url" = "https://github.com/multitheftauto/clang-tools-static-binaries/releases/download/v21.1.7/clang-format-21_linux-amd64"
            "filename" = "clang-format"
            "hash" = "3d75779cdc69b06f7e39946b0e50d6ce3dccda1d766e355bf4cf507b1ae13303"
        }
        "windows-amd64" = @{
            "url" = "https://github.com/multitheftauto/clang-tools-static-binaries/releases/download/v21.1.7/clang-format-21_windows-amd64.exe"
            "filename" = "clang-format.exe"
            "hash" = "11defe615493876745f36fb77e9783e7eed03a7f800044ff58619d9293e79409"
        }
    }
    # "clang-tidy" = @{
    #     "linux-amd64" = @{
    #         "url" = "https://github.com/multitheftauto/clang-tools-static-binaries/releases/download/v21.1.7/clang-tidy-21_linux-amd64"
    #         "filename" = "clang-tidy"
    #         "hash" = "7b35986c04b7c4fc7040c254ac2b5eb0b01ece87c91cccb32e3a4d6ecfa9695f"
    #     }
    #     "windows-amd64" = @{
    #         "url" = "https://github.com/multitheftauto/clang-tools-static-binaries/releases/download/v21.1.7/clang-tidy-21_windows-amd64.exe"
    #         "filename" = "clang-tidy.exe"
    #         "hash" = "e6afe42eb50ec9dcdf945d88b220b6b36971573cec7c5b6db22d3535c6cc61c4"
    #     }
    # }
}

function Get-ClangTool {
    param(
        [Parameter(Mandatory)] [string]$RepoRoot,
        [Parameter(Mandatory)] [string]$ToolName
    )

    $platform = if ($isLinux) { "linux-amd64" } else { "windows-amd64" }
    $toolConfig = $ToolConfig[$ToolName][$platform]
    if (-not $toolConfig) {
        Write-Error "No configuration found for tool '$ToolName' on platform '$platform'"
        exit 1
    }

    $binDir = Join-Path $RepoRoot "Build" "tmp"
    if (-not (Test-Path $binDir)) {
        New-Item -ItemType Directory -Path $binDir | Out-Null
    }

    # Check existing file
    $toolPath = Join-Path $binDir $toolConfig.filename
    if (Test-Path $toolPath) {
        $currentHash = (Get-FileHash -Path $toolPath -Algorithm SHA256).Hash
        if ($currentHash -eq $toolConfig.hash) {
            return $toolPath
        }
        Write-Warning "$ToolName hash mismatch, re-downloading..."
    }

    # Download process
    Write-Host "Downloading $ToolName..." -ForegroundColor Cyan
    Invoke-WebRequest -Uri $toolConfig.url -OutFile $toolPath
    if ($isLinux) {
        chmod +x $toolPath
    }
    Write-Verbose "Downloaded $ToolName to $toolPath"

    # Verify hash
    $downloadedHash = (Get-FileHash -Path $toolPath -Algorithm SHA256).Hash
    if ($downloadedHash -ne $toolConfig.hash) {
        Remove-Item $toolPath -ErrorAction SilentlyContinue
        Write-Error "SHA256 hash mismatch! The download may be corrupted. Expected: $($toolConfig.hash) Got: $downloadedHash"
        exit 1
    }

    Write-Verbose "$ToolName verified successfully."
    return $toolPath
}

function Assert-FormatPath {
    param([string]$RepoRoot, [string]$Path)

    $comparison = if ($IsWindows) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
    $repoPrefix = $RepoRoot + [IO.Path]::DirectorySeparatorChar
    if (-not $Path.StartsWith($repoPrefix, $comparison) -or $Path -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)') {
        throw "Formatter paths must stay inside this repository and outside excluded directories: $Path"
    }
    # A lexical source-root check must not let a link select files elsewhere.
    for ($ancestor = $Path; $ancestor.StartsWith($repoPrefix, $comparison); $ancestor = [IO.Path]::GetDirectoryName($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and
            ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Linked formatter paths are not supported: $ancestor"
        }
    }
}

function Get-SelectedFormatFiles {
    param([string]$RepoRoot, [string[]]$Paths)

    $comparison = if ($IsWindows) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
    $comparer = if ($IsWindows) { [StringComparer]::OrdinalIgnoreCase } else { [StringComparer]::Ordinal }
    $seen = [Collections.Generic.HashSet[string]]::new($comparer)
    $extensions = @('.c', '.cc', '.cpp', '.h', '.hh', '.hpp', '.inl')
    $sourcePrefixes = @('Client', 'Server', 'Shared', 'Tests') | ForEach-Object {
        (Join-Path $RepoRoot $_) + [IO.Path]::DirectorySeparatorChar
    }
    foreach ($candidate in $Paths) {
        if ([string]::IsNullOrWhiteSpace($candidate) -or $candidate -match '[\r\n\0]' -or
            $candidate -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)') {
            throw "Invalid selected formatter path: $candidate"
        }
        $absolute = [IO.Path]::GetFullPath($candidate, $RepoRoot)
        if (-not ($sourcePrefixes | Where-Object { $absolute.StartsWith($_, $comparison) })) {
            throw "Selected formatter files must be inside Client, Server, Shared or Tests: $candidate"
        }
        Assert-FormatPath -RepoRoot $RepoRoot -Path $absolute
        $file = Get-Item -LiteralPath $absolute -Force -ErrorAction Stop
        if ($file -isnot [IO.FileInfo] -or $file.Extension.ToLowerInvariant() -notin $extensions) {
            throw "Selected formatter paths must be supported C/C++ files (including .inl): $candidate"
        }
        if ($seen.Add($file.FullName)) { $file }
    }
}

function Invoke-ClangFormat {
    [CmdletBinding()]
    param([string[]]$Files)

    $arguments = @('-i')
    if ($VerbosePreference -eq 'Continue') {
        $arguments += '--verbose'
    }

    $repoRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
    $scratch = $null
    Push-Location $repoRoot
    Write-Verbose "Changed directory to repository root: $repoRoot"

    try {
        if ($PSBoundParameters.ContainsKey('Files')) {
            # Small batches still use this required utility and its pinned
            # formatter without rewriting unrelated work across the repository.
            $formatFiles = @(Get-SelectedFormatFiles -RepoRoot $repoRoot -Paths $Files)
        } else {
            Write-Verbose "Searching for source files to format..."
            $searchFolders = "Client", "Server", "Shared", "Tests"
            $formatFiles = @(Get-ChildItem -Path $searchFolders -Include *.c, *.cc, *.cpp, *.h, *.hh, *.hpp -Recurse -File -ErrorAction SilentlyContinue)
        }
        if (-not $formatFiles.Count) { throw 'No supported source files were selected for formatting.' }
        $scratchBase = Join-Path $repoRoot 'Build/tmp'
        Assert-FormatPath -RepoRoot $repoRoot -Path $scratchBase
        $clangFormatPath = Get-ClangTool -RepoRoot $repoRoot -ToolName "clang-format"
        # $clangTidyPath = Get-ClangTool -RepoRoot $repoRoot -ToolName "clang-tidy"

        # Keep the response file in a task-owned workspace directory so every
        # success, formatter failure and CI exit can retire it in finally.
        $scratch = Join-Path $scratchBase ('clang-format-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $scratch -ErrorAction Stop | Out-Null
        $tmp = Join-Path $scratch 'files.rsp'
        $responseLines = $formatFiles.FullName | ForEach-Object {
            $responsePath = if ($IsWindows) { $_.Replace('\', '/') } else { $_ }
            '"' + $responsePath.Replace('\', '\\').Replace('"', '\"') + '"'
        }
        [IO.File]::WriteAllLines($tmp, [string[]]$responseLines, [Text.UTF8Encoding]::new($false))
        Write-Verbose "List of files to format written to temporary file: $tmp"

        & "$clangFormatPath" @arguments "@$tmp"
        if ($LASTEXITCODE -ne 0) { throw "clang-format failed with exit code $LASTEXITCODE." }

        Write-Verbose "Successfully formatted $($formatFiles.Count) files."

        # Check git diff only on Linux (CI's checkout starts off clean)
        if ($isLinux) {
            $diffOutput = git diff --name-only
            if ($diffOutput.Count -eq 0) {
                Write-Host "No formatting changes detected." -ForegroundColor Green
                exit 0
            } else {
                Write-Host "::group::Files with formatting issues ($($diffOutput.Count) files)"
                $diffOutput | ForEach-Object { Write-Host $_ }
                Write-Host "::endgroup::"
                Write-Error "Code formatting issues detected. Run utils/clang-format.ps1 locally and commit the changes."
                exit 1
            }
        }
    } finally {
        try {
            if ($scratch) {
                $resolvedScratch = [IO.Path]::GetFullPath($scratch)
                $scratchPrefix = [IO.Path]::GetFullPath((Join-Path $repoRoot 'Build/tmp')) + [IO.Path]::DirectorySeparatorChar
                $comparison = if ($IsWindows) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
                if (-not $resolvedScratch.StartsWith($scratchPrefix, $comparison)) { throw 'Refusing formatter cleanup outside Build/tmp.' }
                Assert-FormatPath -RepoRoot $repoRoot -Path $resolvedScratch
                if (Test-Path -LiteralPath $resolvedScratch) { Remove-Item -LiteralPath $resolvedScratch -Recurse -Force }
            }
        } finally {
            Pop-Location
        }
    }
}

Invoke-ClangFormat @PSBoundParameters
