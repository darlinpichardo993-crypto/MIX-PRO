#pragma once
#include "ParamTable.h"
namespace mp {
inline std::vector<PresetData> factoryPresets() {
    using V = std::vector<std::pair<std::string, float>>;
    std::vector<PresetData> out;
    auto add = [&](const char* cat, const char* name, V v) { PresetData p; p.name = name; p.category = cat; p.values = std::move(v); out.push_back(std::move(p)); };
    enum { Bachata = 0, Soul = 22, Gospel = 23, RnB = 13, Pop = 18, Rock = 19, LatinPop = 7, LatinRnB = 8 };
    // ERA MATCH (starting points; a loaded reference takes priority over the era shape)
    add("ERA MATCH", "Aventura 90s",            {{"mode", kModeEra}, {"role", RhythmGuitar}, {"eraPos", 0.0f}, {"matchAmount", 0.7f}, {"style", 4}, {"ambience", 0.2f}});
    add("ERA MATCH", "Aventura 2000s",          {{"mode", kModeEra}, {"role", LeadGuitar}, {"eraPos", 1.0f}, {"matchAmount", 0.7f}, {"style", 2}, {"ambience", 0.15f}});
    add("ERA MATCH", "Dominican Bachata 90s",   {{"mode", kModeEra}, {"role", RhythmGuitar}, {"eraPos", 0.2f}, {"matchAmount", 0.8f}, {"style", 4}, {"body", 0.2f}});
    add("ERA MATCH", "Romantic Bachata 2000s",  {{"mode", kModeEra}, {"role", LeadGuitar}, {"eraPos", 1.5f}, {"matchAmount", 0.7f}, {"presence", 0.2f}, {"ambience", 0.2f}});
    add("ERA MATCH", "Modern Bachata",          {{"mode", kModeEra}, {"role", LeadGuitar}, {"eraPos", 2.0f}, {"matchAmount", 0.6f}, {"style", 5}});
    add("ERA MATCH", "Vintage Electric Guitar", {{"mode", kModeEra}, {"role", LeadGuitar}, {"eraPos", 0.1f}, {"style", 4}, {"character", 0.25f}, {"attack", 0.2f}});
    add("ERA MATCH", "Modern Electric Guitar",  {{"mode", kModeEra}, {"role", LeadGuitar}, {"eraPos", 2.0f}, {"style", 5}, {"presence", 0.3f}, {"dynamics", 0.2f}});
    // VOCALS
    add("VOCALS", "Bachata Lead",        {{"mode", kModeVocal}, {"role", LeadVocal}, {"genre", Bachata}, {"deEss", 0.1f}, {"ambience", 0.25f}});
    add("VOCALS", "Bachata Second Voice",{{"mode", kModeVocal}, {"role", SecondVoice}, {"genre", Bachata}, {"ambience", 0.3f}});
    add("VOCALS", "Soul Vocal",          {{"mode", kModeVocal}, {"role", LeadVocal}, {"genre", Soul}, {"character", 0.2f}, {"ambience", 0.3f}});
    add("VOCALS", "Gospel Vocal",        {{"mode", kModeVocal}, {"role", LeadVocal}, {"genre", Gospel}, {"presence", 0.2f}, {"ambience", 0.4f}});
    add("VOCALS", "R&B Vocal",           {{"mode", kModeVocal}, {"role", LeadVocal}, {"genre", RnB}, {"ambience", 0.3f}, {"dynamics", 0.2f}});
    add("VOCALS", "Pop Vocal",           {{"mode", kModeVocal}, {"role", LeadVocal}, {"genre", Pop}, {"presence", 0.2f}, {"dynamics", 0.2f}});
    add("VOCALS", "Rock Vocal",          {{"mode", kModeVocal}, {"role", LeadVocal}, {"genre", Rock}, {"character", 0.3f}});
    add("VOCALS", "Choir",               {{"mode", kModeVocal}, {"role", Choir}, {"genre", Gospel}, {"ambience", 0.4f}});
    add("VOCALS", "Backing Vocals",      {{"mode", kModeVocal}, {"role", BackingVocals}, {"genre", Bachata}, {"ambience", 0.3f}});
    // INSTRUMENTS
    add("INSTRUMENTS", "Bachata Guitar",  {{"mode", kModeInstrument}, {"role", LeadGuitar}, {"genre", Bachata}});
    add("INSTRUMENTS", "Rhythm Guitar",   {{"mode", kModeInstrument}, {"role", RhythmGuitar}, {"genre", Bachata}});
    add("INSTRUMENTS", "Lead Guitar",     {{"mode", kModeInstrument}, {"role", LeadGuitar}, {"genre", Bachata}, {"presence", 0.2f}});
    add("INSTRUMENTS", "Bachata Bass",    {{"mode", kModeInstrument}, {"role", Bass}, {"genre", Bachata}});
    add("INSTRUMENTS", "Bongos",          {{"mode", kModeInstrument}, {"role", Bongos}, {"genre", Bachata}});
    add("INSTRUMENTS", "Guira",           {{"mode", kModeInstrument}, {"role", Guira}, {"genre", Bachata}});
    add("INSTRUMENTS", "Piano",           {{"mode", kModeInstrument}, {"role", Piano}, {"genre", Bachata}});
    add("INSTRUMENTS", "Percussion",      {{"mode", kModeInstrument}, {"role", Percussion}, {"genre", Bachata}});
    // MASTER MATCH (loudness targets are user-configurable; these are only starting points)
    add("MASTER", "Streaming",  {{"mode", kModeMaster}, {"mTargetLufs", -14}, {"mCeiling", -1.0f}, {"mMb", 0.4f}, {"mGlue", 0.25f}, {"mMatchLoud", 1}});
    add("MASTER", "CD",         {{"mode", kModeMaster}, {"mTargetLufs", -9}, {"mCeiling", -0.3f}, {"mMb", 0.5f}, {"mGlue", 0.35f}, {"mMatchLoud", 1}});
    add("MASTER", "Loud",       {{"mode", kModeMaster}, {"mTargetLufs", -7}, {"mCeiling", -0.3f}, {"mMb", 0.7f}, {"mGlue", 0.5f}, {"character", 0.2f}, {"mMatchLoud", 1}});
    add("MASTER", "Dynamic",    {{"mode", kModeMaster}, {"mTargetLufs", -16}, {"mCeiling", -1.0f}, {"mMb", 0.2f}, {"mGlue", 0.15f}, {"mMatchLoud", 1}});
    add("MASTER", "Vintage",    {{"mode", kModeMaster}, {"style", 4}, {"mTargetLufs", -12}, {"mCeiling", -0.8f}, {"mMb", 0.3f}, {"mGlue", 0.4f}, {"character", 0.25f}, {"mTilt", -0.8f}, {"mMatchLoud", 1}});
    add("MASTER", "Modern",     {{"mode", kModeMaster}, {"style", 5}, {"mTargetLufs", -10}, {"mCeiling", -0.5f}, {"mMb", 0.6f}, {"mGlue", 0.4f}, {"mTilt", 0.6f}, {"mMatchLoud", 1}});
    add("MASTER", "Bachata",    {{"mode", kModeMaster}, {"mTargetLufs", -10}, {"mCeiling", -0.8f}, {"mMb", 0.4f}, {"mGlue", 0.35f}, {"mTilt", 0.3f}, {"mMonoLow", 100}, {"mMatchLoud", 1}});
    add("MASTER", "Latin Pop",  {{"mode", kModeMaster}, {"mTargetLufs", -9}, {"mCeiling", -0.5f}, {"mMb", 0.5f}, {"mGlue", 0.4f}, {"mTilt", 0.5f}, {"mMonoLow", 100}, {"mMatchLoud", 1}});
    add("MASTER", "R&B",        {{"mode", kModeMaster}, {"mTargetLufs", -11}, {"mCeiling", -0.8f}, {"mMb", 0.4f}, {"mGlue", 0.3f}, {"mMonoLow", 120}, {"mMatchLoud", 1}});
    add("MASTER", "Rock",       {{"mode", kModeMaster}, {"mTargetLufs", -9}, {"mCeiling", -0.5f}, {"mMb", 0.5f}, {"mGlue", 0.5f}, {"character", 0.2f}, {"mMatchLoud", 1}});
    (void)LatinPop; (void)LatinRnB; return out;
}
}
