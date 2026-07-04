# 던전 크롤러 크레딧

## 개발

- QuickBASIC (2026)
- Ospaggi (2026)

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

### PCM 효과음
제작/제공: Ospaggi
- 사용 파일:
  - atk.wav (공격)
  - hit.wav (피격)
  - item.wav (아이템)
  - stair.wav (계단)
  - win.wav (승리)
  - lose.wav (패배)
- 원본: ASSETS/raw/dungeon/sfx/
- 변환: tools/mksfx.ts → SRC/*.SFX

## 음악

### OPL2(AdLib) VGM 트랙
- 작곡/제공: Ospaggi
- 원본: ASSETS/raw/audio_vgm/
- 사용 트랙: TITLE(메뉴), ST00-ST03(지하 1-4층), GAMEOVER(게임오버), ENDING(엔딩)
