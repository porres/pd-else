proc else_colors_open {objectid initialcolor} {
    set color [tk_chooseColor -initialcolor $initialcolor]
    pdsend "$objectid callback $color"
}
