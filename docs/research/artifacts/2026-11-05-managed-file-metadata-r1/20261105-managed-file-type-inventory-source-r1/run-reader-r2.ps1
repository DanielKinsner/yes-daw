Set-StrictMode -Version 3.0
$ErrorActionPreference='Stop'
$PSModuleAutoLoadingPreference='None'
try {
 $inventoryRuntime=[IO.Path]::GetFullPath('C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell')
 if(-not [OperatingSystem]::IsWindows() -or [IntPtr]::Size -ne 8 -or [Environment]::Version.ToString() -cne '10.0.11' -or -not [Environment]::ProcessPath.Equals([IO.Path]::Join($inventoryRuntime,'pwsh.exe'),[StringComparison]::OrdinalIgnoreCase)){throw 'EXACT_METADATA_HOST'}
 $inventoryManifest=[IO.File]::ReadAllBytes([IO.Path]::Join($PSScriptRoot,'source-pins.json'))
 if([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($inventoryManifest)) -cne '8DB54EA7407FA296E658221580644825B532959387EDA287AC75311A5834318C'){throw 'SOURCE_MANIFEST_PIN'}
 Microsoft.PowerShell.Core\Import-Module -Name ([IO.Path]::Join($inventoryRuntime,'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')) -ErrorAction Stop
 $inventoryCompiler=Microsoft.PowerShell.Core\Get-Command -Name 'Microsoft.PowerShell.Utility\Add-Type' -CommandType Cmdlet -ErrorAction Stop
 if($inventoryCompiler.ImplementingType -ne [Microsoft.PowerShell.Commands.AddTypeCommand] -or -not $inventoryCompiler.ImplementingType.Assembly.Location.Equals([IO.Path]::Join($inventoryRuntime,'Microsoft.PowerShell.Commands.Utility.dll'),[StringComparison]::OrdinalIgnoreCase)){throw 'EXACT_COMPILER_CMDLET'}
 $inventoryPins=[Text.UTF8Encoding]::new($false,$true).GetString($inventoryManifest)|Microsoft.PowerShell.Utility\ConvertFrom-Json -AsHashtable
 foreach($inventoryPin in @($inventoryPins.target_inputs)+@($inventoryPins.reader_dependency_file_pins)){
  $inventoryFile=[IO.File]::Open($inventoryPin.path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
  try {if($inventoryFile.Length -ne $inventoryPin.bytes -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($inventoryFile)) -cne $inventoryPin.sha256){throw 'FILE_IDENTITY'}}finally{$inventoryFile.Dispose()}
 }
 $inventorySource=[IO.File]::ReadAllBytes([IO.Path]::Join($PSScriptRoot,'reader.cs'))
 if($inventorySource.Length -ne 18034 -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($inventorySource)) -cne 'E9B1CC2AC4D5B1B3C8D1DE955A30A938E95EADB1FA92C8757350DB07BADACC74'){throw 'CSHARP_SOURCE_PIN'}
 $inventoryReferences=[Collections.Generic.List[string]]::new()
 foreach($inventoryPin in $inventoryPins.reader_dependency_file_pins){$inventoryReferences.Add($inventoryPin.path)}
 $inventoryReferences.Add([IO.Path]::Join($inventoryRuntime,'System.Private.CoreLib.dll'))
 Microsoft.PowerShell.Utility\Add-Type -TypeDefinition ([Text.UTF8Encoding]::new($false,$true).GetString($inventorySource)) -Language CSharp -CompilerOptions @('/optimize+','/warnaserror+','/unsafe-','/checked+') -ErrorAction Stop
 $inventoryExit=[YesDaw.FileMetadataInventory.Program]::Main([string[]]@())
 if($inventoryExit -ne 0 -and $inventoryExit -ne 1){throw 'EXIT_DOMAIN'}
 exit $inventoryExit
}catch{
 [Console]::Error.WriteLine('{"status":"FAIL","exception_type":"RunnerFailure","reason":"METADATA_HOST_OR_COMPILATION"}')
 exit 2
}
