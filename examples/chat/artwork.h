/* Application artwork uses the public, scalable C icon API. */
static cui_icon_asset *brand_art(void) {
  cui_icon_command v[] = {{CUI_ICON_MOVE, {7, 0}},
                          {CUI_ICON_LINE, {15, 0}},
                          {CUI_ICON_CUBIC, {19, 0, 22, 3, 22, 7}},
                          {CUI_ICON_LINE, {22, 15}},
                          {CUI_ICON_CUBIC, {22, 19, 19, 22, 15, 22}},
                          {CUI_ICON_LINE, {7, 22}},
                          {CUI_ICON_CUBIC, {3, 22, 0, 19, 0, 15}},
                          {CUI_ICON_LINE, {0, 7}},
                          {CUI_ICON_CUBIC, {0, 3, 3, 0, 7, 0}},
                          {CUI_ICON_CLOSE},
                          {CUI_ICON_FILL, {0}, theme.foreground, 0},
                          {CUI_ICON_MOVE, {15, 11}},
                          {CUI_ICON_CUBIC, {15, 13.21, 13.21, 15, 11, 15}},
                          {CUI_ICON_CUBIC, {8.79, 15, 7, 13.21, 7, 11}},
                          {CUI_ICON_CUBIC, {7, 8.79, 8.79, 7, 11, 7}},
                          {CUI_ICON_CUBIC, {13.21, 7, 15, 8.79, 15, 11}},
                          {CUI_ICON_CLOSE},
                          {CUI_ICON_FILL, {0}, theme.surface, 0}};
  return cui_icon_vector(22, 22, v, sizeof(v) / sizeof(*v));
}
static cui_icon_asset *shield_art(void) {
  const cui_icon_command v[] = {{CUI_ICON_MOVE, {12, 3}},
                                {CUI_ICON_LINE, {4, 6}},
                                {CUI_ICON_LINE, {4, 12}},
                                {CUI_ICON_CUBIC, {4, 17, 7.5, 20, 12, 21}},
                                {CUI_ICON_CUBIC, {16.5, 20, 20, 17, 20, 12}},
                                {CUI_ICON_LINE, {20, 6}},
                                {CUI_ICON_CLOSE},
                                {CUI_ICON_STROKE, {1.9, 1, 1}, 0, 1},
                                {CUI_ICON_MOVE, {12, 8}},
                                {CUI_ICON_LINE, {12, 12}},
                                {CUI_ICON_STROKE, {1.9, 1, 1}, 0, 1},
                                {CUI_ICON_MOVE, {12, 15.5}},
                                {CUI_ICON_LINE, {12, 15.51}},
                                {CUI_ICON_STROKE, {1.9, 1, 1}, 0, 1}};
  return cui_icon_vector(24, 24, v, sizeof(v) / sizeof(*v));
}
static cui_icon_asset *profile_art(void) {
  cui_icon_command v[] = {{CUI_ICON_MOVE, {28, 14}},
                          {CUI_ICON_CUBIC, {28, 21.73, 21.73, 28, 14, 28}},
                          {CUI_ICON_CUBIC, {6.27, 28, 0, 21.73, 0, 14}},
                          {CUI_ICON_CUBIC, {0, 6.27, 6.27, 0, 14, 0}},
                          {CUI_ICON_CUBIC, {21.73, 0, 28, 6.27, 28, 14}},
                          {CUI_ICON_CLOSE},
                          {CUI_ICON_FILL, {0}, avatar_color("Mathias"), 0},
                          {CUI_ICON_MOVE, {10, 19}},
                          {CUI_ICON_LINE, {10, 9}},
                          {CUI_ICON_LINE, {14, 15}},
                          {CUI_ICON_LINE, {18, 9}},
                          {CUI_ICON_LINE, {18, 19}},
                          {CUI_ICON_STROKE, {1.8, 0, 1}, theme.foreground, 0}};
  return cui_icon_vector(28, 28, v, sizeof(v) / sizeof(*v));
}
static void artwork(cui_widget *w, cui_icon_asset *asset, int size) {
  cui_set_icon(w, asset);
  cui_icon_release(asset);
  cui_set_icon_size(w, size);
}

/* Layout glyphs are application artwork, composed with CUI's vector API. */
static cui_icon_asset *layout_art(unsigned count) {
  cui_icon_command v[20] = {
    {CUI_ICON_MOVE,{6,4}}, {CUI_ICON_LINE,{18,4}},
    {CUI_ICON_CUBIC,{19.1,4,20,4.9,20,6}}, {CUI_ICON_LINE,{20,18}},
    {CUI_ICON_CUBIC,{20,19.1,19.1,20,18,20}}, {CUI_ICON_LINE,{6,20}},
    {CUI_ICON_CUBIC,{4.9,20,4,19.1,4,18}}, {CUI_ICON_LINE,{4,6}},
    {CUI_ICON_CUBIC,{4,4.9,4.9,4,6,4}}, {CUI_ICON_CLOSE},
  };
  size_t n=10;
  if(count>1) {
    float x=count==3?13:12;
    v[n++]=(cui_icon_command){CUI_ICON_MOVE,{x,4}};
    v[n++]=(cui_icon_command){CUI_ICON_LINE,{x,20}};
  }
  if(count>2) {
    v[n++]=(cui_icon_command){CUI_ICON_MOVE,{count==3?13:4,12}};
    v[n++]=(cui_icon_command){CUI_ICON_LINE,{20,12}};
  }
  v[n++]=(cui_icon_command){CUI_ICON_STROKE,{1.8,1,1},0,1};
  return cui_icon_vector(24,24,v,n);
}
