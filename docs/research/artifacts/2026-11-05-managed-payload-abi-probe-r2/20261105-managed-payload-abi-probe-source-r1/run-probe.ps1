# Source-first separate-purpose runner. Root launches this exact script only
# after independent review, in a fresh redirected, hidden, bounded process.
Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$PSModuleAutoLoadingPreference = 'None'

try {
    $abiRuntime = 'C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell'
    $abiPins = @(
        @('pwsh.exe',301368L,'362A356CE7F0940EC74F73A8FC2C990A2CC24A38A11C90BBD8ECA947110AD139'),
        @('pwsh.dll',157496L,'3046ED02928F5D8F78BB3106A861984498D4DC5F80E69A1D5BC7EEB911F0FA90'),
        @('coreclr.dll',4614440L,'C98C3078A4F2495DD7D3CED967C7A6981545194ED163454703580B740F435521'),
        @('clrjit.dll',2092328L,'E7BF743E7BF9A01C8C2BCDC0D828212FD8DFC285BE9D4EC21962BECF456DF648'),
        @('System.Private.CoreLib.dll',16033616L,'ED80A1A5DD066C0407AD59686F9AF90A4F3E55DBF3E4C973B53CC8C9D0706F82'),
        @('pwsh.runtimeconfig.json',790L,'BE41756D6ECCAC092DA616818C83F811C377ADDE6B3696864DD32220770B4D93'),
        @('System.Management.Automation.dll',19646264L,'2D8AF026E589C5603629BFC3E7C57265B4167B12ECA3AE549C6A6C3CA7E4967B'),
        @('Microsoft.PowerShell.Commands.Utility.dll',1652504L,'2F6201BB3CAF4C08158BE733E786DD85B11537694F7E6AB40356FC0727A3596F'),
        @('Microsoft.PowerShell.Commands.Management.dll',1124152L,'40818818DD94A5FA1F53043E33D31D8312ADAD24EF61B06481DFAE4C45D6E677'),
        @('Newtonsoft.Json.dll',1824600L,'9AF3870D53662BA1C8E9B19EC8AD0020E815FFE9E916A1F3204AC6E206447F60'),
        @('Modules/Microsoft.PowerShell.Management/Microsoft.PowerShell.Management.psd1',16076L,'A73617CBA10DF0D2641DF909C6B8BD7981769B3FC78845976290093DF2941941'),
        @('Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1',16826L,'D103110AAE93CA1061FAB8ADD8F00EA6110740AC94B236654D0EFB0B08654AAA'),
        @('Microsoft.CodeAnalysis.CSharp.dll',18536312L,'A1CBB3E005E5F91CBED130EF7EEAA79204E6BB10E05C54CE4C384A81A7646627'),
        @('Microsoft.CodeAnalysis.dll',8128376L,'A11461236D71889C0D7E440E9B08CB2E381F59A16A845648C3809855B6305DCD')
    )
    if (-not [OperatingSystem]::IsWindows() -or [IntPtr]::Size -ne 8 -or
        [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture -ne [Runtime.InteropServices.Architecture]::X64 -or
        [Environment]::Version.ToString() -cne '10.0.11' -or [Runtime.GCSettings]::IsServerGC) {
        throw 'EXACT_WINDOWS_X64_RUNTIME'
    }
    if (-not [Environment]::ProcessPath.Equals([IO.Path]::Join($abiRuntime,'pwsh.exe'),[StringComparison]::OrdinalIgnoreCase) -or
        -not [object].Assembly.Location.Equals([IO.Path]::Join($abiRuntime,'System.Private.CoreLib.dll'),[StringComparison]::OrdinalIgnoreCase) -or
        -not [System.Management.Automation.PSObject].Assembly.Location.Equals([IO.Path]::Join($abiRuntime,'System.Management.Automation.dll'),[StringComparison]::OrdinalIgnoreCase)) {
        throw 'EXACT_CONTROLLER_ASSEMBLY_PATHS'
    }
    $abiArguments = [Environment]::GetCommandLineArgs()
    $abiExpectedArguments = @('<APPHOST>','-NoLogo','-NoProfile','-NonInteractive','-File',$PSCommandPath)
    if ($abiArguments.Length -ne $abiExpectedArguments.Length) { throw 'EXACT_FRESH_ARGUMENT_COUNT' }
    for ($abiIndex = 1; $abiIndex -lt $abiArguments.Length; ++$abiIndex) {
        if (-not $abiArguments[$abiIndex].Equals($abiExpectedArguments[$abiIndex],[StringComparison]::OrdinalIgnoreCase)) {
            throw 'EXACT_FRESH_NO_PROFILE_NONINTERACTIVE_ARGUMENTS'
        }
    }
    $abiApphost = [IO.Path]::GetFullPath($abiArguments[0])
    if (-not $abiApphost.Equals([IO.Path]::Join($abiRuntime,'pwsh.exe'),[StringComparison]::OrdinalIgnoreCase) -and
        -not $abiApphost.Equals([IO.Path]::Join($abiRuntime,'pwsh.dll'),[StringComparison]::OrdinalIgnoreCase)) {
        throw 'EXACT_APPHOST_ARGUMENT'
    }
    foreach ($abiPin in $abiPins) {
        $abiStream = [IO.File]::Open([IO.Path]::Join($abiRuntime,$abiPin[0]),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
        try {
            if ($abiStream.Length -ne $abiPin[1] -or
                [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($abiStream)) -cne $abiPin[2]) {
                throw 'EXACT_PINNED_FILE_BYTES'
            }
        } finally { $abiStream.Dispose() }
    }
    # Public Process.Modules associates the two selected loaded native images;
    # it emits no inventory and never starts another process or thread.
    $abiProcess = [Diagnostics.Process]::GetCurrentProcess()
    try {
        $abiClrCount = 0
        $abiJitCount = 0
        foreach ($abiModule in $abiProcess.Modules) {
            if ($abiModule.ModuleName.Equals('coreclr.dll',[StringComparison]::OrdinalIgnoreCase)) {
                if (-not $abiModule.FileName.Equals([IO.Path]::Join($abiRuntime,'coreclr.dll'),[StringComparison]::OrdinalIgnoreCase)) { throw 'EXACT_LOADED_CORECLR' }
                ++$abiClrCount
            }
            if ($abiModule.ModuleName.Equals('clrjit.dll',[StringComparison]::OrdinalIgnoreCase)) {
                if (-not $abiModule.FileName.Equals([IO.Path]::Join($abiRuntime,'clrjit.dll'),[StringComparison]::OrdinalIgnoreCase)) { throw 'EXACT_LOADED_JIT' }
                ++$abiJitCount
            }
        }
        if ($abiClrCount -ne 1 -or $abiJitCount -ne 1) { throw 'EXACT_NATIVE_RUNTIME_MODULE_COUNTS' }
    } finally { $abiProcess.Dispose() }

    Microsoft.PowerShell.Core\Import-Module -Name ([IO.Path]::Join($abiRuntime,'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')) -ErrorAction Stop
    $abiCompiler = Microsoft.PowerShell.Core\Get-Command -Name 'Microsoft.PowerShell.Utility\Add-Type' -CommandType Cmdlet -ErrorAction Stop
    if ($abiCompiler.ImplementingType -ne [Microsoft.PowerShell.Commands.AddTypeCommand] -or
        -not $abiCompiler.ImplementingType.Assembly.Location.Equals([IO.Path]::Join($abiRuntime,'Microsoft.PowerShell.Commands.Utility.dll'),[StringComparison]::OrdinalIgnoreCase)) {
        throw 'EXACT_ADD_TYPE_CMDLET'
    }
    $abiSourceBytes = [IO.File]::ReadAllBytes([IO.Path]::Join($PSScriptRoot,'probe.cs'))
    if ($abiSourceBytes.Length -ne 15934 -or
        [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($abiSourceBytes)) -cne 'FE0232CE0507AA55435CCD3B880E28F0D9ED05F79C1ECE4DAABD05F454AA2F5E') {
        throw 'EXACT_REVIEWED_CSHARP_SOURCE'
    }
    $abiEncoding = [Text.UTF8Encoding]::new($false,$true)
    $abiSource = $abiEncoding.GetString($abiSourceBytes)
    Microsoft.PowerShell.Utility\Add-Type -TypeDefinition $abiSource -Language CSharp -CompilerOptions @('/optimize+','/warnaserror+','/unsafe-','/checked+') -ErrorAction Stop
    $abiExit = [ManagedPayloadAbiProbeR1]::Run()
    if ($abiExit -ne 0 -and $abiExit -ne 1 -and $abiExit -ne 2) { throw 'BOUNDED_PROBE_EXIT_DOMAIN' }
    exit $abiExit
} catch {
    # No exception message, stack, module inventory, environment value or key.
    $abiExceptionName = $_.Exception.GetType().Name
    if ($abiExceptionName.Length -gt 96) { $abiExceptionName = 'Exception' }
    [Console]::WriteLine('{"kind":"summary","exit":2,"reason":"RUNNER_UNSUPPORTED_OR_EXCEPTION","exception_type":"' + $abiExceptionName + '"}')
    exit 2
}
