# Root-owned compile-only diagnostic. No link or target execution.
$ErrorActionPreference='Stop'
$taskRepo='C:/Users/SM - Dan/Documents/GitHub/yes-daw'
$taskSource=Join-Path $taskRepo 'build-ci/20261105-musical-funded-legacy-adapter-source-r4'
$taskRun=Join-Path $taskRepo 'build-ci/20261105-musical-funded-r4-layout-object-r1/attempt-r1'
if(Test-Path -LiteralPath $taskRun){throw 'Preserve every actual compiler attempt'}
$null=New-Item -ItemType Directory -Path $taskRun
$taskReceiptPath=Join-Path $taskRun 'receipt.json'
$taskReceipt=[ordered]@{exit=1;scope='Compile public R4 sizeof/alignof to constant object data only; no link, target execution, constructor, private-field access, bound price, retained peak or whole qualification';stages=[ordered]@{};commands=[ordered]@{};desktop_input=$false;native_windows=$false;audio_device=$false;target_executed=$false;source_manifest='A5353D86D20DE812A75D6345780C6B489670DD61EB40AF87C4D2CE6776B28077';whole_review='5ED50401AF5207DD32F5E9F854C76A72CF26D40CAFBDA0EC2597ACF7487C108D';compiler_sha256='88C8344236A27A6E727E0A8EDC49AAA2690BDC7A9464B9D18CC7ABE70A9F1C0D'}
. (Join-Path $taskRepo 'build-ci/20261105-click-position-cache-integration-r1/console-helpers.ps1')
$taskCompiler='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe'
Assert-TaskConsole $taskCompiler
if((Get-FileHash -LiteralPath $taskCompiler).Hash -cne $taskReceipt.compiler_sha256){throw 'Compiler changed'}
if((Get-FileHash -LiteralPath (Join-Path $taskSource 'source-pins.json')).Hash -cne $taskReceipt.source_manifest){throw 'Manifest changed'}
if((Get-FileHash -LiteralPath (Join-Path $taskRepo 'build-ci/20261105-musical-funded-legacy-adapter-r4-independent-review/review.json')).Hash -cne $taskReceipt.whole_review){throw 'Whole review changed'}
$taskHeader=Join-Path $taskSource 'native-profile.h'
$taskCpp=Join-Path $taskSource 'native-observer.cpp'
function Assert-NativeObjectSources {
 if((Get-FileHash -LiteralPath $taskHeader).Hash -cne '649BE1643C701FFB987343FB61766BD2F32D8478D28DD9D56BE81BCA85A1EFF9'){throw 'Header changed'}
 if((Get-FileHash -LiteralPath $taskCpp).Hash -cne 'B176E364C7ECB36632C489C74568663500755778AF36ABF70A9C80D00C4F2714'){throw 'Cpp changed'}
}
Assert-NativeObjectSources
Invoke-TaskChild 'environment' $env:ComSpec @('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat') 30 $true
$taskObject=Join-Path $taskRun 'layout.obj'
$taskLayout=Join-Path $taskRepo 'build-ci/20261105-musical-funded-r4-layout-object-r1/layout.cpp'
if((Get-FileHash -LiteralPath $taskLayout).Hash -cne 'FC50B293C2EDE009825392CEB812E84FEF967630C381D3E4875113DF303E9D7A'){throw 'Layout source changed'}
$taskArguments=@('/nologo','/DWIN32','/D_WINDOWS','/EHsc','/O2','/Ob2','/DNDEBUG','/std:c++20','/MD','/W4','/WX','/fp:strict','/Isrc','/c',('/Fo'+$taskObject),$taskLayout)
$taskReceipt.commands['compile']=@{file=$taskCompiler;args=$taskArguments}
Save-TaskReceipt
Invoke-TaskChild 'compile' $taskCompiler $taskArguments 120
Assert-NativeObjectSources
$taskReceipt.object_sha256=(Get-FileHash -LiteralPath $taskObject).Hash
$taskReceipt.exit=0
Save-TaskReceipt
