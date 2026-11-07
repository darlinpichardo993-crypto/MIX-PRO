#pragma once
#include "../DSP/Common.h"
namespace mp {
enum Mode { kModeAuto = 0, kModeReference, kModeEra, kModeGenre, kModeVocal, kModeInstrument, kModeMaster, kModeManual, kNumModes };
enum Role { LeadGuitar = 0, RhythmGuitar, SecondaryGuitar, Bass, Bongos, Guira, LeadVocal, SecondVoice, BackingVocals, VocalDoubles, Harmonies,
            Piano, AcousticGuitar, Drums, Congas, Tambora, Percussion, Synths, Organ, Strings, Brass, Saxophone, Choir, Pads, FX, kNumRoles };
inline bool isVocalRole(int r) { return (r >= LeadVocal && r <= Harmonies) || r == Choir; }

struct RoleBase {
    const char* name; float hpf; float lsF, lsG; float p1F, p1G, p1Q; float p2F, p2G, p2Q; float hsF, hsG;
    float thr, ratio, att, rel, makeup; float sat; float tAtk, tSus; float deEss; float width; float revMix, dlyMix; float monoLow; float matchLo, matchHi;
};
// Starting points only (heuristic). Measured references always take priority in the match engine.
inline const RoleBase* roleTable() {
    static const RoleBase t[kNumRoles] = {
     {"Lead Electric Guitar",100,150,-1.0f,300,-2.0f,1.0f,3200,2.0f,1.1f,8000,1.0f,-20,2.5f,15,120,1.0f,0.20f,0.3f,0.0f,0.00f,1.0f,0.12f,0.06f,0,100,10000},
     {"Rhythm Electric Guitar",110,160,-1.5f,400,-2.5f,1.0f,2800,1.5f,1.0f,7000,0.5f,-18,2.2f,20,140,0.5f,0.15f,0.4f,-0.1f,0.00f,1.2f,0.08f,0.00f,0,100,9000},
     {"Secondary Guitar",140,180,-2.0f,450,-2.0f,1.0f,3500,1.5f,1.2f,9000,1.0f,-18,2.0f,15,120,0.0f,0.15f,0.2f,0.0f,0.00f,1.4f,0.15f,0.10f,0,120,10000},
     {"Bass",30,70,1.0f,250,-2.0f,1.0f,900,1.5f,1.0f,5000,-1.0f,-22,3.5f,20,150,1.5f,0.30f,0.2f,0.1f,0.00f,0.0f,0.00f,0.00f,0,30,5000},
     {"Bongos",150,200,-1.0f,400,-2.0f,1.0f,4500,2.5f,1.0f,9000,1.0f,-20,2.5f,5,80,1.0f,0.10f,0.5f,-0.2f,0.00f,1.1f,0.10f,0.00f,0,150,12000},
     {"Guira",400,400,-2.0f,1500,-1.5f,1.0f,6000,-2.0f,1.5f,12000,1.0f,-22,2.2f,3,60,1.0f,0.05f,0.3f,-0.2f,0.25f,1.3f,0.05f,0.00f,0,400,16000},
     {"Lead Vocal",90,200,-1.0f,350,-2.0f,1.2f,3500,2.5f,0.9f,11000,2.0f,-22,3.0f,12,100,2.0f,0.15f,0.0f,0.0f,0.45f,1.0f,0.18f,0.10f,0,100,14000},
     {"Second Voice",110,220,-1.5f,400,-2.0f,1.2f,3000,2.0f,1.0f,10000,1.5f,-20,3.0f,10,100,2.0f,0.12f,0.0f,0.0f,0.40f,1.1f,0.22f,0.12f,0,120,13000},
     {"Backing Vocals",150,250,-2.0f,500,-2.5f,1.0f,3000,1.5f,1.0f,9000,1.0f,-22,3.5f,8,90,2.0f,0.10f,0.0f,0.0f,0.35f,1.5f,0.30f,0.12f,0,150,12000},
     {"Vocal Doubles",140,220,-2.0f,450,-2.0f,1.0f,4000,1.0f,1.0f,9500,0.5f,-20,3.0f,10,100,2.0f,0.10f,0.0f,0.0f,0.40f,1.6f,0.12f,0.05f,0,140,13000},
     {"Harmonies",160,250,-2.0f,500,-2.0f,1.0f,3500,1.0f,1.0f,9000,1.0f,-22,3.0f,10,100,2.0f,0.10f,0.0f,0.0f,0.35f,1.6f,0.35f,0.15f,0,150,12000},
     {"Piano",70,120,-1.0f,300,-1.5f,1.0f,3000,1.5f,1.0f,9000,1.0f,-22,2.0f,20,150,0.0f,0.05f,0.0f,0.0f,0.00f,1.3f,0.15f,0.00f,0,60,12000},
     {"Acoustic Guitar",90,150,-1.0f,250,-2.0f,1.0f,4000,2.0f,1.0f,10000,1.5f,-20,2.2f,15,120,0.0f,0.05f,0.1f,0.0f,0.00f,1.2f,0.12f,0.00f,0,90,12000},
     {"Drums",40,90,1.5f,400,-3.0f,1.2f,5000,2.0f,1.0f,10000,1.5f,-20,3.0f,20,120,1.0f,0.20f,0.3f,0.0f,0.00f,1.1f,0.08f,0.00f,100,40,14000},
     {"Congas",100,150,0.0f,350,-1.5f,1.0f,4000,2.0f,1.0f,8000,0.5f,-20,2.5f,8,90,0.0f,0.10f,0.4f,-0.1f,0.00f,1.1f,0.10f,0.00f,0,100,12000},
     {"Tambora",60,100,1.0f,300,-2.0f,1.0f,3500,2.0f,1.0f,8000,0.0f,-20,3.0f,10,80,0.0f,0.15f,0.4f,0.0f,0.00f,1.0f,0.08f,0.00f,0,60,10000},
     {"Percussion",200,250,-1.0f,800,-1.0f,1.0f,5000,1.5f,1.0f,10000,1.0f,-20,2.2f,6,80,0.0f,0.08f,0.3f,-0.1f,0.10f,1.3f,0.10f,0.00f,0,200,16000},
     {"Synths",80,150,-1.0f,400,-1.5f,1.0f,3000,1.0f,1.0f,9000,1.0f,-20,2.0f,20,150,0.0f,0.10f,0.0f,0.0f,0.00f,1.4f,0.15f,0.05f,0,80,14000},
     {"Organ",80,150,-1.0f,400,-1.5f,1.0f,2500,1.0f,1.0f,8000,0.5f,-20,2.0f,20,150,0.0f,0.10f,0.0f,0.0f,0.00f,1.2f,0.15f,0.00f,0,80,10000},
     {"Strings",100,150,-1.0f,400,-1.5f,1.0f,3500,1.0f,1.0f,10000,1.0f,-24,1.8f,25,180,0.0f,0.00f,0.0f,0.0f,0.00f,1.5f,0.25f,0.00f,0,100,14000},
     {"Brass",120,200,-1.0f,500,-1.5f,1.0f,2500,1.5f,1.0f,8000,0.5f,-20,2.5f,12,100,0.0f,0.10f,0.1f,0.0f,0.10f,1.1f,0.12f,0.00f,0,120,12000},
     {"Saxophone",100,180,-1.0f,450,-1.5f,1.0f,3000,1.5f,1.0f,9000,1.0f,-20,2.5f,12,110,0.0f,0.08f,0.0f,0.0f,0.10f,1.1f,0.15f,0.05f,0,100,12000},
     {"Choir",120,250,-2.0f,500,-2.0f,1.0f,3500,1.5f,1.0f,10000,1.5f,-22,2.5f,12,110,0.0f,0.05f,0.0f,0.0f,0.30f,1.6f,0.35f,0.00f,0,120,14000},
     {"Pads",120,200,-2.0f,500,-2.0f,1.0f,3000,0.0f,1.0f,9000,0.0f,-22,1.5f,25,180,0.0f,0.00f,0.0f,0.0f,0.00f,1.8f,0.25f,0.00f,0,120,14000},
     {"FX",100,150,0.0f,500,0.0f,1.0f,3000,0.0f,1.0f,9000,0.0f,-20,1.5f,20,150,0.0f,0.00f,0.0f,0.0f,0.00f,1.5f,0.10f,0.05f,0,100,16000}};
    return t;
}
inline RoleBase neutralBase() {
    return RoleBase{"Neutral", 0, 120, 0, 400, 0, 1, 3000, 0, 1, 8000, 0, 0, 1, 20, 150, 0, 0, 0, 0, 0, 1, 0, 0, 0, 20, 20000};
}
inline const char* roleNames() {
    return "Lead Electric Guitar|Rhythm Electric Guitar|Secondary Guitar|Bass|Bongos|Guira|Lead Vocal|Second Voice|Backing Vocals|Vocal Doubles|Harmonies|Piano|Acoustic Guitar|Drums|Congas|Tambora|Percussion|Synths|Organ|Strings|Brass|Saxophone|Choir|Pads|FX";
}

struct StyleScale { float eq, comp, sat, highDb; };
inline StyleScale styleScale(int s) {
    static const StyleScale t[6] = { {0.6f, 0.7f, 0.6f, 0.f}, {0.5f, 0.6f, 0.5f, 0.f}, {1.f, 1.f, 1.f, 0.f}, {1.4f, 1.4f, 1.5f, 0.f}, {0.9f, 1.f, 1.6f, -2.f}, {1.f, 1.2f, 0.8f, 1.5f} };
    return t[clampT(s, 0, 5)];
}
inline const char* styleNames() { return "Natural|Conservative|Auto|Aggressive|Vintage|Modern"; }
}
