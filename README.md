Gamecube emulator with dynamic recompilation(qemu arch)



CPU is 80% done and runs at 75MHz (15.53% of original)
For now rendering is purely on host cpu
![MIT License](media/progress-screenshot-04.gif)



//Animal crossing is loading and running at ~2fps with the software renderer
![MIT License](media/progress-screenshot-05.png)

TODO RENDER:
  - [ ] transformation
      - [ ] frac in pos
  - [ ] Texture mapping 
  - [ ] color channels 
  - [ ] effects
  - [ ] post processing 

TODO Optimization
 - [ ] tb-chainging
 - [ ] global memory maanager (allocate in bigger chunks)

 - [x] use another thread for the command processor



