# Bible Verse Viewer — Flipper Zero App


Luther Bibel 1912 - Bible study app for Flipper Zero
A full-featured offline Bible reader for the Flipper Zero. Browse, search, and bookmark verses from SD card files.

---

<img width=19% height=19% alt="main_menu" src="https://github.com/user-attachments/assets/adba9650-59cb-4244-9824-15803f617e8a" />

<img width=19% height=19% alt="settings" src="https://github.com/user-attachments/assets/f1e7dfb7-2389-4d36-b111-9c525f247bf2" />

<img width=19% height=19% alt="reading_screen" src="https://github.com/user-attachments/assets/67025c0a-360c-4788-a454-990ee0bd7cf3" />

<img width=19% height=19% alt="search" src="https://github.com/user-attachments/assets/577dfa42-b276-4fef-a669-5b45f467a99c" />

<img width=19% height=19% alt="search_results" src="https://github.com/user-attachments/assets/601ac89e-8943-4937-94b9-987936ef7470" />


---


**Credits:** I used Custom Font files from [JBlanked's](https://github.com/jblanked) project here... [Hello-World-Flipper-Zero](https://github.com/jblanked/Hello-World-Flipper-Zero).

This app was built with the assistance of [Claude](https://www.google.com/aclk?sa=L&pf=1&ai=DChsSEwj2ouHcyO6SAxVpFK0GHeImMJIYACICCAEQARoCcHY&co=1&ase=2&gclid=Cj0KCQiA7-rMBhCFARIsAKnLKtCsQdQJL2gmzqz5jsZBJ7hhI2l-HVuQ3-vNUiLQEz--k6bTNDZTz1EaArLgEALw_wcB&cid=CAASWuRop1HLx_wJXV_6YNoiswCR_CMK1oeLWmejtkjGwuQatm9-O6-DLRbSDIJ5DeArBQzVyEnGCyjfMfuNvLcrhULbSAVmO0jon8KWGZYhFK9_yxti0CDO2Z4tfA&cce=2&category=acrcp_v1_32&sig=AOD64_0rqbc12ct-DZqYYuzELr-Tdhr7gA&q&nis=4&adurl=https://chaton.ai/claude/?utm_source%3Dgoogle%26utm_medium%3Dcpc%26utm_campaign%3DGA%2520%7C%2520ChatOn%2520%7C%2520Web%2520%7C%2520US%2520%7C%2520Search%2520%7C%2520Main%2520%7C%2520CPA%2520%7C%252021.07.25%26utm_content%3D790508279447%26utm_term%3Dclaude%2520ai%26campaign_id%3D22809916767%26adset_id%3D186149762341%26ad_id%3D790508279447%26gad_source%3D1%26gad_campaignid%3D22809916767%26gbraid%3D0AAAAA9SXzF5Rcu5MIgUy86oFRsGg2F56b%26gclid%3DCj0KCQiA7-rMBhCFARIsAKnLKtCsQdQJL2gmzqz5jsZBJ7hhI2l-HVuQ3-vNUiLQEz--k6bTNDZTz1EaArLgEALw_wcB&ved=2ahUKEwjy_9rcyO6SAxUGOjQIHcy8ItMQ0Qx6BAgoEAE)


---

**Requires:** SD card with the sd_files (`.txt` format)

---

## Features

### Offline

| Feature | Description |
|---|---|
| **Browse** | Scroll through all verses in the loaded file |
| **Search** | Full-text keyword search across all verses |
| **5 Font Sizes** | Tiny (4×6), Small (5×8), Medium (6×10), Large (9×15), Flipper built-in |

---

## Controls

| Button | Action |
|---|---|
| **Up / Down** | Navigate menus; scroll verse text and search results |
| **Left / Right** | Cycle Book / Chapter / Verse in the quick picker; switch settings |
| **Up (long)** | Fill suggested word |
| **Left / Right (long)** | Cycle through input suggestions |
| **OK (short)** | Select menu item; type character on keyboard; confirm action |
| **OK (long)** | Toggle caps lock; bookmark the current verse |
| **Back (short)** | Return to previous screen; backspace in text input |
| **Back (long)** | show font list in Settings |

---

## SD Card Setup

Place verse files in `/ext/apps_data/luther1912/`.

```
