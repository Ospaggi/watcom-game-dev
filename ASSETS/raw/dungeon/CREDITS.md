# 던전 크롤러 크레딧

## 개발

- QuickBASIC (2026)

# 에셋 출처

## 그래픽

### DawnLike 16x16 Universal Rogue-like Tileset v1.81
- 출처: https://opengameart.org/content/dawnlike-16x16-universal-rogue-like-tileset-v181
- 작가: DragonDePlatino (팔레트: DawnBringer)
- 라이선스: CC-BY 4.0 — 크레딧 필수: "DawnLike tileset by DragonDePlatino, palette by DawnBringer"
- 사용 파일: Floor.png, Wall.png, Tile.png, Potion.png, MedWep.png, Money.png,
  Rodent0.png, Pest0.png, Humanoid0.png, Undead0.png, Slime0.png, Player0.png,
  Reptile0.png, Avian0.png
- 변환: tools/mksheet.ts (TILES.json/MOBS.json 셀 좌표) → mksprite → SRC/TILES.SPR, SRC/MOBS.SPR

## 효과음

### The Essential Retro Video Game Sound Effects Collection (512 sounds)
- 출처: https://opengameart.org/content/512-sound-effects-8-bit-style
- 작가: Juhani Junkala (SubspaceAudio)
- 라이선스: CC0 (퍼블릭 도메인)
- 사용 파일:
  - atk.wav ← Weapons/Melee/sfx_wpn_sword2.wav (공격)
  - hit.wav ← General Sounds/Simple Damage Sounds/sfx_damage_hit2.wav (피격)
  - item.wav ← General Sounds/Positive Sounds/sfx_sounds_powerup1.wav (아이템)
  - stair.wav ← Movement/Climbing Stairs/sfx_movement_stairs1a.wav (계단)
  - win.wav ← General Sounds/Fanfares/sfx_sounds_fanfare1.wav (승리)
  - lose.wav ← General Sounds/Negative Sounds/sfx_sounds_error1.wav (패배)
- 변환: tools/mksfx.ts → SRC/*.SFX

## 음악

### OPL2(AdLib) VGM 트랙 (ST00.VGM ~ ST07.VGM)
- 작곡/제공: Ospaggi 님 (도스박물관)
- 원본: ASSETS/raw/audio_vgm/
- 사용 트랙: ST00(메뉴), ST01~ST03(지하 1~3층), ST06(지하 4층), ST07(엔딩)
