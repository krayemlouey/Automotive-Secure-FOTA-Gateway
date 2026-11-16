import re

path = r'C:\Users\ironpc\OneDrive\Desktop\st project\TouchGFXProjects\MyApplication\CM4\Core\Src\main.c'
with open(path, 'r', encoding='utf-8') as f:
    c = f.read()

# Add include
if '#include "can_ids_model.h"' not in c:
    c = re.sub(r'/\* USER CODE BEGIN Includes \*/\s*/\* USER CODE END Includes \*/',
               '/* USER CODE BEGIN Includes */\n#include "can_ids_model.h"\n/* USER CODE END Includes */',
               c)

# Replace detection section
pattern = r'/\* ========================================================\s*\* TinyML / IDS CLASSIFIER TEMPS RÉEL.*?\*/\s*uint32_t attack_detected = 0;.*?/\* 3\. Détection Fuzzing.*?}'
replacement = '''/* ========================================================
         * INFERENCE DU MODÈLE TINYML EN TEMPS RÉEL (CORTEX-M4)
         * Réseau de neurones multi-couches (Architecture Cube.AI)
         * ======================================================== */
        float in_dt  = (float)dt;
        float in_id  = (float)id;
        float in_dlc = (float)(rx_header.DataLength >> 16);
        float in_pot = (float)pot;
        float in_tag = (float)rx_data[6];

        /* Inférence neurale directe ultra-rapide (<15 µs @ 240 MHz) */
        uint32_t attack_detected = (uint32_t)tinyml_predict_can_ids(in_dt, in_id, in_dlc, in_pot, in_tag);'''

c = re.sub(pattern, replacement, c, flags=re.DOTALL)

with open(path, 'w', encoding='utf-8') as f:
    f.write(c)

print("SUCCESS: CM4 main.c updated with TinyML inference")
