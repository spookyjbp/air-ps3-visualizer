# AIR PS3 artwork

`artwork-source.png` contains the supplied artwork's decoded RGB pixels in a
lossless PNG with source metadata removed. The in-app boot screen background
(`background.rgb`, shown for a moment before the animated UI starts and used
as the fallback screen) is generated from it.

`ICON0.PNG` is the XMB icon: a 320x176 PNG with a transparent background,
supplied by the fork owner.

No `PIC1.PNG` is shipped, so the console's normal XMB background stays
visible while AIR PS3 is highlighted. The release gate checks the icon size
and that no `PIC1.PNG` is present.
