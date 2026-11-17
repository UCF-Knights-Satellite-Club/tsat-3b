# Existing PDB connectors and their cable mates

**No extra PCB connectors or electronic components were added for this review.** The installed headers cover the PDB's power/control interfaces. The items below are matching cable ends. Reuse matching supplied leads where available.

| PDB reference | Installed board part | Correct cable mate / termination | Connection by PCB pin number |
|---|---|---|---|
| J1 battery | Two 2.2 mm solder holes | Deans/T-plug pigtail that physically mates with the actual Ovonic battery; solder and strain-relieve to board | 1 battery positive; 2 GND. This is not an XT30 battery input. |
| J2 main controller PCB | JST **S4B-XH-A(LF)(SN)**, XH 4-way, 2.50 mm | **XHP-4** housing + four **SXH-001T-P0.6** contacts | 1 +5V; 2 GND; 3 CUT_PWM; 4 VBAT_SENSE. |
| J3 Waveshare power and servo data | JST **S3B-PH-K-S(LF)(SN)**, PH 3-way, 2.00 mm | **PHR-3** housing + three **SPH-002T-P0.5S** contacts | 1 ADAPTER_7V2; 2 GND; 3 SERVO_D. Split the cable as explained below. |
| J4 cutter arming link | AMASS **XT30UPB-M30** PCB male | Cable female **XT30U-F** with a short, insulated conductor joining its two contacts | 1 +5V; 2 CUT_PWR. This is an arming bridge, not a normal positive/GND supply port. |
| J5 nichrome output | AMASS **XT30UPB-M30** PCB male | Cable female **XT30U-F**, two leads to nichrome terminals | 1 HEATER_PLUS; 2 HEATER_LOW (switched return). |
| J6 camera PCB | JST **S2B-XH-A(LF)(SN)**, XH 2-way, 2.50 mm | **XHP-2** housing + two **SXH-001T-P0.6** contacts | 1 +5V; 2 GND. |
| J7 / J8 servo power + D | Molex **0436500315**, Micro-Fit 3-way, 3.00 mm | **0436450300** housing + three **0430300007** female contacts per connector | 1 GND; 2 fused 7.2V; 3 shared servo D. |
| J9 external brake resistors | Two 2.2 mm solder holes | Soldered, insulated and strain-relieved leads to the external resistor bank | 1 VSERVO_7V2; 2 BRAKE_DRAIN. Pin 2 is not GND. |
| JP1 main ON | Samtec **TSW-102-07-G-S**, two pins, 2.54 mm | Standard two-position 2.54 mm jumper; e.g. **SPC02SYAN**, or an existing matching jumper | Joins the input-FET gate-bias circuit to GND; carries gate-bias current, not load current. |

**XH (2.50 mm), PH (2.00 mm), Micro-Fit (3.00 mm), and 2.54 mm jumper housings are different families and are not interchangeable.** The JST XH contacts listed accept 22–28 AWG; use 22 AWG for their power conductors. The PH contacts accept 24–30 AWG; use 24 AWG on the adapter branch. The listed Micro-Fit contacts accept 20–24 AWG. Check the insulation diameter as well as conductor gauge against the contact drawing.

## Waveshare cable: two separate interfaces

J3.1 goes to the green input terminal marked positive, and J3.2 to its negative terminal. J3.3 goes to **D only** on a Waveshare servo port. Leave that servo port's V/G contacts unconnected in this split harness; the servos receive power directly from J7/J8. Adapter power therefore stays within the small F5 branch rather than carrying both servo currents through the adapter.

The **ESP32 still connects directly to the Waveshare UART TX/RX/GND header** with a separate three-wire cable. This does not require another PDB header. For this specific adapter, Waveshare documents **ESP TX → adapter TX, ESP RX → adapter RX**, common ground, with the mode jumper in **A**. Follow those silk labels; do not apply the usual crossed-UART assumption without checking this adapter's documentation.

Reuse matching female jumper leads if already supplied. If assembling a three-position 2.54 mm UART cable, **Harwin M20-1060300** housing with three **M20-1160042** contacts is a suitable family for standard 0.64 mm square header pins; those contacts accept 22–30 AWG. Secure any unlatched cable mechanically. The controller-board end depends on the controller PCB header, which was not supplied in this PDB-only review.

## Servo-end connector

The specified **Feetech ST-3215-C044** datasheet identifies the factory connection as **5264-3P**, with **1 GND, 2 Vcc, 3 Signal/TTL**. This agrees electrically with J7/J8's pin numbering, but the Micro-Fit plug does not fit directly into the servo.

Use the factory servo cable ends in the adapter harness where possible. For a newly built 2.50 mm Mini-SPOX/5264 cable end, the matching family is **Molex 0050375033** three-way housing with **0008701040** loose tin female contacts (22–28 AWG). Verify the actual delivered servo's connector/keying against the C044 drawing before assembly. A short **22 AWG** cable fits both the listed Micro-Fit and Mini-SPOX contacts. Alternatively, retain a 20 AWG Micro-Fit power section and splice properly to the factory tail; do not force 20 AWG wire into a Mini-SPOX contact rated for 22–28 AWG, or a thin factory wire into an oversized crimp.

The Mini-SPOX factory interface retains its own rating. Replacing the PDB header does not make the complete factory harness an 8.5 A continuous assembly. The normal one-active-servo operating plan is retained.

The J2/J6 **PDB-side** mating parts above are definite. Matching connectors and pin ordering on the separate custom main/camera PCBs must agree with this map. GPS, IMU, radio, SD and camera data connect on those boards; they do not each require another PDB connector.

## Manufacturer references

- JST XH: <https://www.jst-mfg.com/product/pdf/eng/eXH.pdf>
- JST PH: <https://www.jst-mfg.com/product/pdf/eng/ePH.pdf>
- Molex Micro-Fit header: <https://www.molex.com/en-us/products/part-detail/0436500315>
- Molex 5264 housing: <https://www.molex.com/en-us/products/part-detail/50375033>
- Molex 5263 loose contact: <https://www.molex.com/en-us/products/part-detail/8701040>
- Feetech C044, pages 4 and 8: <https://files.seeedstudio.com/products/Feetech/101090141_Feetech_ST-3215-C044_Datasheet.pdf>
- Waveshare UART wiring: <https://docs.waveshare.com/Bus_Servo_Adapter_A/Product-Wiring-Example>
- Harwin UART cable housing: <https://www.harwin.com/products/M20-1060300>
- Harwin contact: <https://www.harwin.com/products/M20-1160042>
