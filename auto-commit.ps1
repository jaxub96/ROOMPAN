git add .

if (git diff --cached --quiet) {
    Write-Host "No changes to save"
}
else {
    git commit -m "auto save $(Get-Date)"
    git push
}