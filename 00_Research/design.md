# Design File
This file is responsible for initial thoughts on design

![Track](track.png)

## table of contents
- [Design Questions](#Design-Questions)
- [Other Cars notes](#Other-Cars-notes)


## Design Questions
- **Will a camera be used to recognize the traffic signs (red and green pillars)? Is it a USB-camera or a native device (like the Raspberry Pi camera)?** </br></br>
 Raspberry Pi Cam </br></br></br>

- **How fast can the video stream from the camera be handled? Which controller will give you suitable performance?** </br></br>
 Raspberry Pi </br></br></br>

 - **Will the vehicle be controlled by one controller or by a main controller and an auxiliary controller?** </br></br>
 Raspberry Pi (Main) & (Arduino or ESP32) </br></br></br>

- **How will the roles between controllers be distributed:** </br>
    - **handling of the video stream**: </br></br>
        Raspberry Pi </br></br></br>
    - **direct control of the motors** </br></br>
        ESP32 </br></br></br>
    - **direct acquisition of sensor readings?** </br></br>
        ESP32 </br></br></br>
    - **Will the capacity (input/output pins) of each controller be enough for this?** </br></br>
        Really HOPE SO </br></br></br>
    - **What kind of connection will there be between the controllers?** </br></br>
        my initial thoughts</br>
        1- I2C: I don't think it's a good idea, limited and needed for sensors with feedback</br>
        **2- UART: Maybe is a good idea, faster than I2C and is a good way since there are more than one port on the Pi**</br>
        3- Serial-USB Connection: I don't think it's recommended </br>
        4- Are there any recommendations?? </br></br></br>

- **Will the walls be detected by camera or by distance/proximity sensors?**
    - **Camera:** </br>
    1- Obstacles Color (Red or Green) </br>
    2- Maybe lines </br> </br>

    - **Sensors** </br>
    1- Obstacles </br>
    2- Track Edges (use the advantage of colour black) </br>
    3- Distances (either obstacle distance or the distance the car moved) </br>
    4- if the car is in the center or too close to the edge </br>
    5- Speed Estimation </br></br>

    Of course there will be integration between both for better results </br></br></br>

- **Should a special lens kit be used to make the camera's field of view wider to recognize both traffic signs and walls or will several cameras be used instead?** </br></br>

    One wide camera is enough I think (270 degree) </br></br></br>

- **How will the traversed path be measured:** </br></br>

    1- by counting numbers of turns with a compass (IDK I think it may be affected by magnetic field especially near sensors) </br>
    2- gyroscope (Good?)</br>
    3- by calculating the distance with an encoder (good for distance but for angles IDK) </br>
    4- by detecting colored lines with light sensors (maybe hard to count on a little bit) </br></br>

    I think of using the gyroscope as the primary thing, and detecting colors to **Correct** the gyroscope </br></br></br>

- **Will a gyroscope and/or an accelerometer be used to assist in accurate driving, to get rid of excessive steering mechanism shifts?** </br></br>

    I think yeah why not </br></br></br>
    
    1- by counting numbers of turns with a compass (IDKKK i think it may be effected by magnatic field esspicially near sensors) </br>
    2- gyroscope (Good ?)</br>
    3- by calculating the distance with an encoder (good for distance but for angles idk) </br>
    4- by detecting colored lines with light sensors (maybe hard to count on a little bit) </br></br>

    I think of use the gyroscope as primary thing, and detecting colors to **Correct** The gyroscope </br></br></br>

- **Will a gyroscope and/or an accelerometer be used to assist in accurate driving, to get rid of excessive steering mechanism shifts?** </br></br>

    I think ya why not </br></br></br>

- **What kind of power sources should be used to supply controllers, motors, and sensors? Should it be one source for all of these, or should there be dedicated sources for controllers and motors** </br></br>

    **Raspberry Pi**: Power bank, USB-C ...etc </br>
    **Arduino/ESP32**: Lithium-Ion or Lithium-Polymer </br>
    **Motors**: DC Battery (9V, 7.4V, 11.1V etc....) </br>
    **Sensors**: From Arduino/ESP32 3.3V, 5V for light sensors, Battery if needed </br>
    **Motors**: DC Battery (9v, 7.4v, 11.1v etc....) </br>
    **Sensors**: From Arduino/ESP32 3.3v, 5v for light sensors, Battery if needed </br>



## Other Cars notes

#### Car1: Lor7 team

**Github Repo Link**:https://github.com/Lor7/WRO2024-FUTURE-ENGINEERS-VOLTERRA-TEAM-INTERNATIONAL-FINAL </br></br>

##### Car Body:
Three **plywood** parts, 2 big middle layers, 1 small upper layer </br></br>
**Raw Car** </br>
![raw_car](./Others/Cars/Lor7/raw_car.png) </br></br>
**Car with components** </br>
![raw_car](./Others/Cars/Lor7/car_with_comp.png) </br></br>
**the plywoods** </br>
![raw_car](./Others/Cars/Lor7/woods.png) </br></br>
**more photos** </br>
![raw_car](./Others/Cars/Lor7/woods2.png) </br></br>


---
##### Steering system:
They used **ackerman** steering system, where it prevents the car from slipping and help the cat be stabalize </br>
**ackerman** steering system video explain: https://www.youtube.com/watch?v=yXfTY5y3SAM </br>
Another explain in 3D car: https://www.youtube.com/shorts/Orw6h8SkGTg
their ackerman: </br>
![raw_car](./Others/Cars/Lor7/buttom.png) </br></br>


**Front**: </br>
![Falafel-Front](./Others/Cars/Falafel/Front.png)

**Left**: </br>
![Falafel-Front](./Others/Cars/Falafel/Left.png)

**Right**: </br>
![Falafel-Front](./Others/Cars/Falafel/Right.png)

**Back**: </br>
![Falafel-Front](./Others/Cars/Falafel/Back.png)

**Top**: </br>
![Falafel-Front](./Others/Cars/Falafel/Top.png)

**Buttom**: </br>
![Falafel-Front](./Others/Cars/Falafel/Buttom.png)

**Schematic**: </br>
![Falafel-Front](./Others/Cars/Falafel/Schematic.jpeg)

**Wiring**: </br>
![Falafel-Front](./Others/Cars/Falafel/wiring.jpeg)

---
---

#### Car2: Falafel Team

**Github Repo Link**: https://github.com/FalafelTech-team/WRO-Future-Engineers

1- Three Ultra-sonic (left right front) </br>
2- Fixed (Ras. PI) camera </br>
3- Rasbperri PI & Arduino (mega) </br>
4- Arduino communicate with Raspberry PI through its serial </br>
5- 3D printing </br>
6- (wooden or solid central block with 3d Printing) </br>
7- There's A lego </br>
8- Large Wheels </br>
9- Left and Right Are symmetrical </br> </br>

**Front**: </br>
![Falafel-Front](./Others/Cars/Falafel/Front.png)

**Left**: </br>
![Falafel-Front](./Others/Cars/Falafel/Left.png)

**Right**: </br>
![Falafel-Front](./Others/Cars/Falafel/Right.png)

**Back**: </br>
![Falafel-Front](./Others/Cars/Falafel/Back.png)

**Top**: </br>
![Falafel-Front](./Others/Cars/Falafel/Top.png)

**Buttom**: </br>
![Falafel-Front](./Others/Cars/Falafel/Buttom.png)

**Schematic**: </br>
![Falafel-Front](./Others/Cars/Falafel/Schematic.jpeg)

**Wiring**: </br>
![Falafel-Front](./Others/Cars/Falafel/wiring.jpeg)

---
---

#### Car3: E4Force Team

**Github Repo Link**: https://github.com/E4Force/E4-Force-Team/tree/main

**Front**: </br>
![E4Force-Front](./Others/Cars/E4Force/Front.png)

**Left**: </br>
![E4Force-Front](./Others/Cars/E4Force/Left.png)

**Right**: </br>
![E4Force-Front](./Others/Cars/E4Force/Right.png)

**Back**: </br>
![E4Force-Front](./Others/Cars/E4Force/Back.png)

**Buttom**: </br>
![E4Force-Front](./Others/Cars/E4Force/Buttom.png)

---
---

#### Car4: reiruso07 Team
</br>
Btw i liked thier documantiation
</br>
1- three Ultra sonic sensors </br>
2- Canon cam </br>
3- Raspberriy PI & Arduino </br>
4- Power bank to feed the PI </br> 
5- 2 wooden Blocks </br>


**Github Repo Link**: https://github.com/reiruso07/WRO2025_Future_Engineers-Team-Violet/ </br>

**Front**: </br>
![reiruso07-Front](./Others/Cars/reiruso07/Front.png)

**Left**: </br>
![reiruso07-Front](./Others/Cars/reiruso07/Left.png)

**Right**: </br>
![reiruso07-Front](./Others/Cars/reiruso07/Right.png)

**Back**: </br>
![reiruso07-Front](./Others/Cars/reiruso07/Back.png)

**Buttom**: </br>
![reiruso07-Front](./Others/Cars/reiruso07/Buttom.png)

**Schematic**: </br>
![reiruso07-Front](./Others/Cars/reiruso07/Schematic.jpg)

---
---

#### Car5: asalos Team

**Github Repo Link**: https://github.com/asalos22222/WRO-FUTURE-ENGINEERS-2024 </br>

1- 3 Ultra Sonic </br>
2- Pi Cam </br>
3- raspberri Pi with ESP32 </br>

**Front**: </br>
![asalos-Front](./Others/Cars/asalos/Front.png)

**Left**: </br>
![asalos-Front](./Others/Cars/asalos/Left.png)

**Right**: </br>
![asalos-Front](./Others/Cars/asalos/Right.png)

**Back**: </br>
![asalos-Front](./Others/Cars/asalos/Back.png)

**Buttom**: </br>
![asalos-Front](./Others/Cars/asalos/Buttom.png)

**Schematic**: </br>
![asalos-Front](./Others/Cars/asalos/Schematic.jpeg)

**3D**: </br>
![asalos-Front](./Others/Cars/asalos/3D.jpeg)


---
---

#### Car6: DriverUS Team

**Github Repo Link**: https://github.com/World-Robot-Olympiad-Association-FE/2024-Int-Final-DriverUS/tree/main </br>

**Front**: </br>
![DriverUS-Front](./Others/Cars/DriverUS/Front.png)

**Left**: </br>
![DriverUS-Front](./Others/Cars/DriverUS/Left.png)

**Right**: </br>
![DriverUS-Front](./Others/Cars/DriverUS/Right.png)

**Back**: </br>
![DriverUS-Front](./Others/Cars/DriverUS/Back.png)

**Buttom**: </br>
![DriverUS-Front](./Others/Cars/DriverUS/Buttom.png)