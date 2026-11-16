# Script de flashage automatique pour NUCLEO-F446RE
$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$bin = "C:\Users\ironpc\OneDrive\Desktop\st project\LOUEY\F446RE_CAN_TX\Debug\F446RE_CAN_TX.bin"

if (-not (Test-Path $cli)) {
    Write-Host "[ERREUR] STM32_Programmer_CLI non trouve !" -ForegroundColor Red
    pause
    exit 1
}

if (-not (Test-Path $bin)) {
    Write-Host "[ERREUR] F446RE_CAN_TX.bin non trouve ! Veuillez compiler d'abord." -ForegroundColor Red
    pause
    exit 1
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Flashage NUCLEO-F446RE (ECU Simulateur + Generateur IA)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

& $cli -c port=SWD -d $bin 0x08000000 -rst

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n[SUCCES] NUCLEO-F446RE flashee avec succes !" -ForegroundColor Green
} else {
    Write-Host "`n[ECHEC] Verifiez que la carte NUCLEO-F446RE est bien branchee en USB." -ForegroundColor Red
}
