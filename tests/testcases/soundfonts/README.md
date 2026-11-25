The synthesis tests write their own SF2 and SF3 containers around a generated 440 Hz sine wave. The SF2 sample is synthesized by NumPy. The SF3 sample uses this checked-in Ogg Vorbis stream so the tests need no encoder.

```sh
ffmpeg -f lavfi -i 'aevalsrc=0.3662109375*sin(2*PI*440*t):s=22050:d=1' -c:a libvorbis -q:a 4 sine.ogg
```
