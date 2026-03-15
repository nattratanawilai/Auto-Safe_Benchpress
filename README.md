Smart Safety Bar for Bench Press

" An Automatic Safety System for  Weightlifters "
 
โปรเจกต์โครงงานพัฒนาระบบความปลอดภัยอัตโนมัติสำหรับผู้ที่ออกกำลังกายท่า Bench Press คนเดียว โดยใช้เซนเซอร์วัดระยะทางความแม่นยำสูง (ToF) วิเคราะห์ความเร็วและพฤติกรรมของบาร์เบล เพื่อสั่งการระบบกลไกช่วยเหลือเมื่อผู้เล่นเกิดอาการหมดแรงหรือเกิดอุบัติเหตุ

ปัจจุบันโปรเจกต์อยู่ในเฟสทดสอบและประมวลผลอัลกอริทึม (Algorithm Validation) โดยเน้นไปที่ความแม่นยำของการอ่านค่าเซนเซอร์และตรรกะความปลอดภัย เมื่อระบบตรวจพบเงื่อนไขอันตรายจะจำลองการสั่งงานโดยการแสดงผลผ่าน Serial Monitor เป็นหลัก (ยังไม่ได้เชื่อมต่อระบบมอเตอร์จริงในเวอร์ชันนี้)
 
Developer:[ณัฏฐ์ รัตนวิไล , เตชินท์ พรหมเจริญ , ณัฐนันท์  คงมาก]

School:[โรงเรียนแสงทองวิทยา]

Core Algorithms (ระบบประมวลผลหลัก) :

 1.Dynamic Peak Tracking:ระบบไม่มีการ Fix ระยะตายตัว แต่จะทำ Auto-Calibration หาจุดสูงสุด (Unrack Height) ของผู้เล่นแต่ละคนแบบ Real-time ในทุกๆ เซ็ต ทำให้รองรับผู้เล่นได้ทุกความสูงและทุกความยาวแขน

 2.Noise & Ghost Velocity Filtering:ใช้ Low-pass filter และการทำ Time Interval (50ms) เพื่อป้องกันค่าความเร็วแกว่ง จากสัญญาณรบกวนของเซนเซอร์ I2C

 3. 3-Tier Safety State Machine:ตัดสินใจสั่งการด้วยตรรกะ 3 ระดับ:

  T1 (Sticking Point):ตรวจจับจังหวะหมดแรงดันไม่ขึ้น (Velocity อยู่ในช่วง -0.15 ถึง 0.15 m/s นานเกิน 1.5 - 2.0 วินาที)

  T2 (Rapid Drop):ตรวจจับบาร์เบลร่วงหลุดมือ (Velocity < -0.80 m/s)

  T3 (Unconscious):ตรวจจับบาร์เบลทับหน้าอกค้างไว้นานเกิน 4 วินาที


Hardware Setup (อุปกรณ์ที่ใช้) :

 Microcontroller:ESP32-S3 
 
 Sensors:6x VL53L1X (Time-of-Flight Distance Sensors)
 
 I2C Multiplexer:TCA9548A 
 
 Input/Output: Foot Pedal , Relay Module ควบคุม Solenoid Valve


I2C Pin Configuration:

 | Module | ESP32-S3 Pin | Note |

 | SDA | GPIO 5 | Data Line |
 
 | SCL | GPIO 6 | Clock Line |
 
 | MUX RST| GPIO 4 | Active Low for Hardware Reset |
 
 | Pedal | GPIO 12 | Input Signal |

Repository Structure :

 /src - Source code หลักของระบบ 
 
 /docs - ไฟล์ Flowchart แผนผังการทำงานของ Algorithm 

Current Scope & Future Work :
Phase 1: Logic & Algorithm (Current) มุ่งเน้นการแก้ปัญหาเรื่องสัญญาณรบกวน (Noise Filtering) และสร้าง State Machine ที่ทำงานได้แม่นยำ 100% โดยแสดงผลการ Trigger ผ่าน Serial Console

Phase 2: Hardware Actuation (Future Plan)
  เตรียมนำoutput จากบอร์ด ESP32 ไปเชื่อมต่อกับ Relay Module เพื่อสั่งมอเตอร์จริง


