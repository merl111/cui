#import <AppKit/AppKit.h>
#include <stdio.h>
static unsigned pixels(NSBitmapImageRep *rep) {
  unsigned n=0; unsigned char *p=[rep bitmapData];
  for(int i=0;i<200*50;i++) if(p[i*4+3]) n++;
  return n;
}
int main(void) { @autoreleasepool {
 [NSApplication sharedApplication];
 for(int premult=0;premult<2;premult++) for(int flipped=0;flipped<2;flipped++) {
  NSBitmapImageRep *rep=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:200 pixelsHigh:50 bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:premult?0:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:800 bitsPerPixel:32];
  [NSGraphicsContext saveGraphicsState];
  NSGraphicsContext *ctx=[NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
  [NSGraphicsContext setCurrentContext:ctx];
  if(flipped){ NSAffineTransform *flip=[NSAffineTransform transform]; [flip translateXBy:0 yBy:50]; [flip scaleXBy:1 yBy:-1]; [flip concat]; }
  [@"Visible text" drawInRect:NSMakeRect(0,0,160,24) withAttributes:@{NSFontAttributeName:[NSFont systemFontOfSize:16],NSForegroundColorAttributeName:[NSColor redColor]}];
  [[NSColor blueColor] setFill]; [[NSBezierPath bezierPathWithRect:NSMakeRect(180,10,10,10)] fill];
  printf("premult=%d flipped=%d context=%d before=%u ",premult,flipped,ctx!=nil,pixels(rep));
  [ctx flushGraphics]; printf("flush=%u ",pixels(rep));
  [NSGraphicsContext restoreGraphicsState]; printf("restore=%u\n",pixels(rep));
  [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[NSString stringWithFormat:@"probe-%d-%d.png",premult,flipped] atomically:YES];
  [rep release];
 }
} return 0; }
