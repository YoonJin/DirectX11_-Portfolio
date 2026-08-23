# pack_items.ps1 - one-off tool (phase 8): packs image/0.png .. 11.png into one sprite sheet + atlas definition.
#
#   Output : image/items_atlas.png  (4 columns x 3 rows, 255x255 cells -> 1020x765, no padding)
#            image/items_atlas.txt  (text definition: "texture <path>" / "frame <name> <x> <y> <w> <h> <pivotX> <pivotY>")
#   Run    : powershell -ExecutionPolicy Bypass -File image\pack_items.ps1   (from the repo root, or anywhere - paths are script-relative)
#
# Why a build-time tool and not runtime packing: the game only ever reads one sheet; packing belongs to the asset pipeline
# (TexturePacker etc. in production). Idempotent - running it twice produces byte-identical outputs.
#
# No padding between cells: the runtime samples with PointClamp and exact texel rectangles, so no bleeding. If the atlas is
# ever sampled with a linear filter, adjacent cells can bleed into each other at the edges - add a 1-2px transparent gutter then.
#
# Pixels are copied with LockBits as raw BGRA bytes, NOT with Graphics.DrawImage. Even with CompositingMode = SourceCopy,
# GDI+ DrawImage runs the pixels through its premultiplied pipeline and pixels with alpha 1..2 come back with their RGB
# rounded to 255 - the straight-alpha data must reach the runtime untouched (WIC premultiplies on load, once). LockBits copies
# the bytes verbatim. (The only thing the PNG encoder still changes is the hidden RGB of fully transparent pixels, which
# is irrelevant after premultiplication.)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$dir     = Split-Path -Parent $MyInvocation.MyCommand.Path   # image/
$cols    = 4
$rows    = 3
$cell    = 255
$count   = 12
$pivotX  = 127      # bottom centre of the icon = the "feet" of a character sprite
$pivotY  = 255

$fmt   = [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
$sheet = New-Object System.Drawing.Bitmap ($cols * $cell), ($rows * $cell), $fmt
$sheetData = $sheet.LockBits((New-Object System.Drawing.Rectangle 0, 0, $sheet.Width, $sheet.Height), [System.Drawing.Imaging.ImageLockMode]::WriteOnly, $fmt)
$rowBytes = $cell * 4
$row = New-Object byte[] $rowBytes

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# packed from image/0.png .. $($count - 1).png by pack_items.ps1 ($($cols)x$($rows), $($cell)x$($cell) cells, no padding)")
$lines.Add("# frame <name> <x> <y> <w> <h> [<pivotX> <pivotY>]   - texel coordinates, pivot relative to the frame's top-left")
$lines.Add("texture image/items_atlas.png")

# try/finally so a throw (wrong-size icon) never leaves a bitmap locked - matters when the script is dot-sourced interactively.
try {
    for ($i = 0; $i -lt $count; $i++) {
        $src = [System.Drawing.Bitmap]::FromFile((Join-Path $dir "$i.png"))
        $srcData = $null
        try {
            if ($src.Width -ne $cell -or $src.Height -ne $cell) { throw "$i.png is $($src.Width)x$($src.Height), expected $($cell)x$($cell)" }
            $x = ($i % $cols) * $cell
            $y = [math]::Floor($i / $cols) * $cell
            # raw row-by-row copy (source bitmaps are re-read as 32bppArgb so the byte layout matches regardless of the PNG's own format)
            $srcData = $src.LockBits((New-Object System.Drawing.Rectangle 0, 0, $cell, $cell), [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $fmt)
            for ($r = 0; $r -lt $cell; $r++) {
                [System.Runtime.InteropServices.Marshal]::Copy([IntPtr][long]($srcData.Scan0.ToInt64() + [long]$r * $srcData.Stride), $row, 0, $rowBytes)
                [System.Runtime.InteropServices.Marshal]::Copy($row, 0, [IntPtr][long]($sheetData.Scan0.ToInt64() + [long]($y + $r) * $sheetData.Stride + [long]$x * 4), $rowBytes)
            }
            $lines.Add(("frame item_{0:d2} {1,4} {2,4} {3} {4}  {5} {6}" -f $i, $x, $y, $cell, $cell, $pivotX, $pivotY))
        } finally {
            if ($srcData) { $src.UnlockBits($srcData) }
            $src.Dispose()
        }
    }
} finally {
    $sheet.UnlockBits($sheetData)
}

try {
    $sheet.Save((Join-Path $dir 'items_atlas.png'), [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $sheet.Dispose()
}

# ASCII, CRLF, no BOM - the runtime parser (TextureAtlas::LoadFromFile) reads it as narrow text.
[System.IO.File]::WriteAllText((Join-Path $dir 'items_atlas.txt'), (($lines -join "`r`n") + "`r`n"), (New-Object System.Text.UTF8Encoding $false))
Write-Host "wrote items_atlas.png ($($cols * $cell)x$($rows * $cell)) and items_atlas.txt ($count frames)"
