$env:pythonpath = $env:pythonpath -replace "Python27", "Python37"
$env:path = $env:path -replace "Python27", "Python37"
$env:pythonpath
.\opendatacon
exit