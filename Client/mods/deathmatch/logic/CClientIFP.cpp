/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Client/mods/deathmatch/logic/CClientIFP.cpp
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include <StdInc.h>
#include <game/CAnimBlendHierarchy.h>
#include <game/CAnimBlendSequence.h>
#include <game/CAnimManager.h>
#include "CIFPAnimationStorage.h"
#include "CIFPReadGuard.h"
#include <limits>
#include <new>

CClientIFP::CClientIFP(class CClientManager* pManager, ElementID ID) : CClientIFP(pManager, ID, *g_pGame->GetAnimManager())
{
}

CClientIFP::CClientIFP(class CClientManager* pManager, ElementID ID, CAnimManager& animationManager) : CClientEntity(ID)
{
    // Init
    SetSmartPointer(true);
    m_pManager = pManager;
    SetTypeName("IFP");
    m_pAnimManager = &animationManager;
    m_pIFPAnimations = std::make_shared<CIFPAnimations>(*m_pAnimManager);
    m_pVecAnimations = &m_pIFPAnimations->vecAnimations;
    m_bVersion1 = false;
    m_bUnloading = false;
    m_u32Hashkey = 0;
}

void CClientIFP::Unlink()
{
    std::shared_ptr<CClientIFP> pIFP = g_pClientGame->GetIFPPointerFromMap(m_u32Hashkey);
    if (pIFP)
    {
        // Remove IFP from map, so we can indicate that it does not exist
        g_pClientGame->RemoveIFPPointerFromMap(m_u32Hashkey);

        // Remove IFP animations from replaced animations of peds/players
        g_pClientGame->OnClientIFPUnload(pIFP);
    }
}

bool CClientIFP::Load(SString blockName, bool isRawData, SString input)
{
    // A retry must not inherit an old offset or ANPK version bit. Public
    // animation lookups keep using the old published set until full success.
    FreeFileReaderMemory();
    m_bVersion1 = false;
    bool                            success = false;
    std::shared_ptr<CIFPAnimations> candidate;
    try
    {
        candidate = std::make_shared<CIFPAnimations>(*m_pAnimManager);
        m_pVecAnimations = &candidate->vecAnimations;
        if (isRawData)
            MtaGame::IfpRead::RequireBytes(input.size(), (std::numeric_limits<std::uint32_t>::max)());
        const bool loaded = isRawData ? CFileReader::LoadFromBuffer(std::move(input)) : CFileReader::LoadFromFile(std::move(input));
        if (loaded)
        {
            MtaGame::IfpRead::RequireBytes(GetLoadedBytesCount(), (std::numeric_limits<std::uint32_t>::max)());
            success = ReadIFPByVersion();
        }
        if (success)
        {
            const auto hash = HashString(blockName.ToLower());
            m_strBlockName.swap(blockName);
            m_u32Hashkey = hash;
            m_pIFPAnimations.swap(candidate);
        }
    }
    catch (const std::exception&)
    {
        success = false;
    }
    m_pVecAnimations = &m_pIFPAnimations->vecAnimations;
    FreeFileReaderMemory();
    // A failed candidate releases only its own arrays/interfaces/frames. The
    // old shared set can remain retained by existing animation associations.
    candidate.reset();
    return success;
}

bool CClientIFP::ReadIFPByVersion()
{
    // Check if we have enough bytes in memory to check the version
    if (GetRemainingBytesCount() < 4)
        return false;

    char Version[4];
    ReadBytes(Version, sizeof(Version));

    bool bAnp3 = strncmp(Version, "ANP3", sizeof(Version)) == 0;
    bool bAnp2 = strncmp(Version, "ANP2", sizeof(Version)) == 0;
    bool bAnpk = strncmp(Version, "ANPK", sizeof(Version)) == 0;

    if (bAnp2 || bAnp3)
    {
        ReadIFPVersion2(bAnp3);
        return true;
    }
    else if (bAnpk)
    {
        m_bVersion1 = true;
        ReadIFPVersion1();
        return true;
    }
    return false;
}

void CClientIFP::ReadIFPVersion1()
{
    SInfo Info{};
    ReadHeaderVersion1(Info);

    if (Info.Entries < 0)
        throw std::runtime_error("Negative IFP animation count");
    MtaGame::IfpRead::RequireBytes(MtaGame::IfpRead::Product(static_cast<std::size_t>(Info.Entries), sizeof(SBase)), GetRemainingBytesCount());
    m_pVecAnimations->resize(static_cast<std::size_t>(Info.Entries));
    for (auto& Animation : *m_pVecAnimations)
    {
        ReadAnimationNameVersion1(Animation);

        SDgan Dgan;
        ReadDgan(Dgan);

        Animation.pHierarchy = m_pAnimManager->GetCustomAnimBlendHierarchy();
        InitializeAnimationHierarchy(Animation.pHierarchy, Animation.Name, Dgan.Info.Entries);
        AllocateSequencesMemory(Animation);
        ReadSequencesWithDummies(Animation);
        AttachSequencesMemory(Animation);
        PreProcessAnimationHierarchy(Animation.pHierarchy);
    }
}

void CClientIFP::ReadIFPVersion2(bool bAnp3)
{
    SIFPHeaderV2 Header;
    ReadBuffer<SIFPHeaderV2>(&Header);

    if (Header.TotalAnimations < 0)
        throw std::runtime_error("Negative IFP animation count");
    MtaGame::IfpRead::RequireBytes(
        MtaGame::IfpRead::Product(static_cast<std::size_t>(Header.TotalAnimations), sizeof(SAnimationHeaderV2::Name) + sizeof(std::int32_t)),
        GetRemainingBytesCount());
    m_pVecAnimations->resize(static_cast<std::size_t>(Header.TotalAnimations));
    for (auto& Animation : *m_pVecAnimations)
    {
        SAnimationHeaderV2 AnimationNode;
        ReadAnimationHeaderVersion2(AnimationNode, bAnp3);

        Animation.Name = AnimationNode.Name;
        Animation.uiNameHash = HashString(Animation.Name.ToLower());
        Animation.pHierarchy = m_pAnimManager->GetCustomAnimBlendHierarchy();
        InitializeAnimationHierarchy(Animation.pHierarchy, AnimationNode.Name, AnimationNode.TotalObjects);

        AllocateSequencesMemory(Animation);
        ReadSequencesWithDummies(Animation);
        AttachSequencesMemory(Animation);
        PreProcessAnimationHierarchy(Animation.pHierarchy);
    }
}

WORD CClientIFP::ReadSequencesWithDummies(SAnimation& animation)
{
    SequenceMapType MapOfSequences;
    WORD            wUnknownSequences = ReadSequences(animation, MapOfSequences);

    MoveSequencesWithDummies(animation, MapOfSequences, animation.AnimatedBonesMask);
    WORD cSequences = m_kcIFPSequences + wUnknownSequences;

    // As we need support for all 64 bones (32 ped + 32 cutscene ped), we must change the total sequences count
    animation.pHierarchy->SetNumSequences(cSequences);
    return cSequences;
}

WORD CClientIFP::ReadSequences(SAnimation& animation, SequenceMapType& MapOfSequences)
{
    if (m_bVersion1)
    {
        return ReadSequencesVersion1(animation, MapOfSequences);
    }
    return ReadSequencesVersion2(animation, MapOfSequences);
}

WORD CClientIFP::ReadSequencesVersion1(SAnimation& animation, SequenceMapType& MapOfSequences)
{
    WORD wUnknownSequences = 0;
    for (size_t SequenceIndex = 0; SequenceIndex < animation.pHierarchy->GetNumSequences(); SequenceIndex++)
    {
        SAnim        Anim;
        std::int32_t iBoneID = ReadSequenceVersion1(Anim);
        if (Anim.Frames == 0)
        {
            continue;
        }

        std::unique_ptr<CAnimBlendSequence> pAnimationSequence;
        bool                                bUnknownSequence = iBoneID == eBoneType::UNKNOWN;
        if (bUnknownSequence)
        {
            size_t UnkownSequenceIndex = m_kcIFPSequences + wUnknownSequences;
            pAnimationSequence = m_pAnimManager->GetCustomAnimBlendSequenceAt(animation.pSequences, static_cast<DWORD>(UnkownSequenceIndex));
            wUnknownSequences++;
        }
        else
        {
            pAnimationSequence = m_pAnimManager->GetCustomAnimBlendSequence();
        }
        MtaGame::IfpStorage::SequenceOwner<CAnimManager> owner(*m_pAnimManager, std::move(pAnimationSequence), !bUnknownSequence);
        auto&                                            sequence = owner.Get();
        if (!sequence)
            throw std::runtime_error("IFP backend returned no sequence");
        InitializeAnimationSequence(sequence, Anim.Name, iBoneID);
        owner.MarkInitialized();

        eFrameType iFrameType = ReadKfrm();
        if ((ReadSequenceKeyFrames(sequence, iFrameType, Anim.Frames)) && (!bUnknownSequence))
        {
            MapOfSequences.insert_or_assign(iBoneID, std::move(owner));
        }
    }
    return wUnknownSequences;
}

WORD CClientIFP::ReadSequencesVersion2(SAnimation& animation, SequenceMapType& MapOfSequences)
{
    WORD wUnknownSequences = 0;
    for (size_t SequenceIndex = 0; SequenceIndex < animation.pHierarchy->GetNumSequences(); SequenceIndex++)
    {
        SSequenceHeaderV2 ObjectNode;
        ReadSequenceVersion2(ObjectNode);

        bool bUnknownSequence = ObjectNode.BoneID == eBoneType::UNKNOWN;

        std::unique_ptr<CAnimBlendSequence> pAnimationSequence;
        if (bUnknownSequence)
        {
            size_t UnkownSequenceIndex = m_kcIFPSequences + wUnknownSequences;
            pAnimationSequence = m_pAnimManager->GetCustomAnimBlendSequenceAt(animation.pSequences, static_cast<DWORD>(UnkownSequenceIndex));
            wUnknownSequences++;
        }
        else
        {
            pAnimationSequence = m_pAnimManager->GetCustomAnimBlendSequence();
        }
        MtaGame::IfpStorage::SequenceOwner<CAnimManager> owner(*m_pAnimManager, std::move(pAnimationSequence), !bUnknownSequence);
        auto&                                            sequence = owner.Get();
        if (!sequence)
            throw std::runtime_error("IFP backend returned no sequence");
        InitializeAnimationSequence(sequence, ObjectNode.Name, ObjectNode.BoneID);
        owner.MarkInitialized();

        eFrameType iFrameType = static_cast<eFrameType>(ObjectNode.FrameType);
        if ((ReadSequenceKeyFrames(sequence, iFrameType, ObjectNode.TotalFrames)) && (!bUnknownSequence))
        {
            MapOfSequences.insert_or_assign(ObjectNode.BoneID, std::move(owner));
        }
    }
    return wUnknownSequences;
}

std::int32_t CClientIFP::ReadSequenceVersion1(SAnim& Anim)
{
    SCpan Cpan;
    ReadBuffer<SCpan>(&Cpan);
    RoundSize(Cpan.Base.Size);
    ReadBytes(&Anim, sizeof(SBase));
    RoundSize(Anim.Base.Size);
    if (Anim.Base.Size > sizeof(SAnim) - sizeof(SBase) || Anim.Base.Size < sizeof(Anim.Name) + sizeof(Anim.Frames))
        throw std::runtime_error("Invalid IFP sequence chunk size");
    std::memset(&Anim.Name, 0, sizeof(SAnim) - sizeof(SBase));
    ReadBytes(&Anim.Name, Anim.Base.Size);
    Anim.Name[sizeof(Anim.Name) - 1] = '\0';

    SString      strBoneName = ConvertStringToKey(Anim.Name, true);
    std::int32_t iBoneID = GetBoneIDFromName(strBoneName);

    SString strCorrectBoneName = GetCorrectBoneNameFromName(strBoneName);
    strncpy(Anim.Name, strCorrectBoneName, strCorrectBoneName.size() + 1);
    return iBoneID;
}

void CClientIFP::ReadSequenceVersion2(SSequenceHeaderV2& ObjectNode)
{
    ReadBuffer<SSequenceHeaderV2>(&ObjectNode);
    ObjectNode.Name[sizeof(ObjectNode.Name) - 1] = '\0';
    SString strBoneName = ConvertStringToKey(ObjectNode.Name);
    SString strCorrectBoneName;
    if (ObjectNode.BoneID == eBoneType::UNKNOWN)
    {
        ObjectNode.BoneID = GetBoneIDFromName(strBoneName);
        strCorrectBoneName = GetCorrectBoneNameFromName(strBoneName);
    }
    else
    {
        strCorrectBoneName = GetCorrectBoneNameFromID(ObjectNode.BoneID);
        if (strCorrectBoneName.size() == 0)
        {
            strCorrectBoneName = GetCorrectBoneNameFromName(strBoneName);
        }
    }

    strncpy(ObjectNode.Name, strCorrectBoneName, strCorrectBoneName.size() + 1);
}

bool CClientIFP::ReadSequenceKeyFrames(std::unique_ptr<CAnimBlendSequence>& pAnimationSequence, eFrameType iFrameType, const std::int32_t& cFrames)
{
    if (cFrames < 0)
        throw std::runtime_error("Negative IFP frame count");
    if (cFrames == 0)
        return false;

    size_t iCompressedFrameSize = GetSizeOfCompressedFrame(iFrameType);
    size_t iSourceFrameSize = GetSourceFrameDataSize(iFrameType);
    if (iCompressedFrameSize == 0 || iSourceFrameSize == 0)
        throw std::runtime_error("Unsupported IFP frame type");
    const auto sourceBytes = MtaGame::IfpRead::Product(static_cast<std::size_t>(cFrames), iSourceFrameSize);
    const auto compressedBytes = MtaGame::IfpRead::Product(static_cast<std::size_t>(cFrames), iCompressedFrameSize);
    MtaGame::IfpRead::RequireBytes(sourceBytes, GetRemainingBytesCount());

    BYTE* pKeyFrames = m_pAnimManager->AllocateKeyFramesMemory(static_cast<std::uint32_t>(compressedBytes));
    if (!pKeyFrames)
    {
        // Advance stream past frame data to keep subsequent reads aligned
        SkipBytes(sourceBytes);
        return false;
    }

    MtaGame::IfpStorage::AttachFrames(*m_pAnimManager, *pAnimationSequence, static_cast<std::size_t>(cFrames), IsKeyFramesTypeRoot(iFrameType),
                                      m_kbAllKeyFramesCompressed, pKeyFrames);
    ReadKeyFramesAsCompressed(pAnimationSequence, iFrameType, cFrames);
    return true;
}

void CClientIFP::ReadHeaderVersion1(SInfo& Info)
{
    std::uint32_t OffsetEOF;
    ReadBuffer<std::uint32_t>(&OffsetEOF);
    RoundSize(OffsetEOF);
    ReadBytes(&Info, sizeof(SBase));
    RoundSize(Info.Base.Size);
    if (Info.Base.Size < sizeof(Info.Entries) || Info.Base.Size > sizeof(SInfo) - sizeof(SBase))
        throw std::runtime_error("Invalid IFP INFO chunk size");
    ReadBytes(&Info.Entries, Info.Base.Size);
}

void CClientIFP::ReadAnimationNameVersion1(SAnimation& IfpAnimation)
{
    SName Name;
    ReadBuffer<SName>(&Name);
    RoundSize(Name.Base.Size);

    char szAnimationName[24];
    if (Name.Base.Size > sizeof(szAnimationName))
        throw std::runtime_error("Invalid IFP animation name size");
    ReadStringNullTerminated(szAnimationName, Name.Base.Size);
    IfpAnimation.Name = szAnimationName;
    IfpAnimation.uiNameHash = HashString(IfpAnimation.Name.ToLower());
}

void CClientIFP::ReadDgan(SDgan& Dgan)
{
    ReadBytes(&Dgan, sizeof(SBase) * 2);
    RoundSize(Dgan.Base.Size);
    RoundSize(Dgan.Info.Base.Size);
    if (Dgan.Info.Base.Size < sizeof(Dgan.Info.Entries) || Dgan.Info.Base.Size > sizeof(SInfo) - sizeof(SBase))
        throw std::runtime_error("Invalid IFP DGAN INFO size");
    ReadBytes(&Dgan.Info.Entries, Dgan.Info.Base.Size);
}

CClientIFP::eFrameType CClientIFP::ReadKfrm()
{
    SKfrm Kfrm;
    ReadBuffer<SKfrm>(&Kfrm);
    return GetFrameTypeFromFourCC(Kfrm.Base.FourCC);
}

void CClientIFP::ReadAnimationHeaderVersion2(SAnimationHeaderV2& AnimationNode, bool bAnp3)
{
    ReadStringNullTerminated((char*)&AnimationNode.Name, sizeof(SAnimationHeaderV2::Name));
    ReadBuffer<std::int32_t>(&AnimationNode.TotalObjects);
    if (bAnp3)
    {
        ReadBuffer<std::int32_t>(&AnimationNode.FrameSize);
        ReadBuffer<std::int32_t>(&AnimationNode.isCompressed);
    }
}

void CClientIFP::ReadKeyFramesAsCompressed(std::unique_ptr<CAnimBlendSequence>& pAnimationSequence, eFrameType iFrameType, const std::int32_t& cFrames)
{
    switch (iFrameType)
    {
        case eFrameType::KRTS:
        {
            ReadKrtsFramesAsCompressed(pAnimationSequence, cFrames);
            break;
        }
        case eFrameType::KRT0:
        {
            ReadKrt0FramesAsCompressed(pAnimationSequence, cFrames);
            break;
        }
        case eFrameType::KR00:
        {
            ReadKr00FramesAsCompressed(pAnimationSequence, cFrames);
            break;
        }
        case eFrameType::KR00_COMPRESSED:
        {
            ReadCompressedFrames<SCompressed_KR00>(pAnimationSequence, cFrames);
            break;
        }
        case eFrameType::KRT0_COMPRESSED:
        {
            ReadCompressedFrames<SCompressed_KRT0>(pAnimationSequence, cFrames);
            break;
        }
    }
}

void CClientIFP::ReadKrtsFramesAsCompressed(std::unique_ptr<CAnimBlendSequence>& pAnimationSequence, const std::int32_t& cFrames)
{
    for (std::int32_t FrameIndex = 0; FrameIndex < cFrames; FrameIndex++)
    {
        SCompressed_KRT0* CompressedKrt0 = static_cast<SCompressed_KRT0*>(pAnimationSequence->GetKeyFrame(FrameIndex, sizeof(SCompressed_KRT0)));
        SKrts             Krts;
        ReadBuffer<SKrts>(&Krts);

        CompressedKrt0->Rotation.X = static_cast<int16_t>(((-Krts.Rotation.X) * 4096.0f));
        CompressedKrt0->Rotation.Y = static_cast<int16_t>(((-Krts.Rotation.Y) * 4096.0f));
        CompressedKrt0->Rotation.Z = static_cast<int16_t>(((-Krts.Rotation.Z) * 4096.0f));
        CompressedKrt0->Rotation.W = static_cast<int16_t>((Krts.Rotation.W * 4096.0f));
        CompressedKrt0->Time = static_cast<int16_t>((Krts.Time * 60.0f + 0.5f));
        CompressedKrt0->Translation.X = static_cast<int16_t>((Krts.Translation.X * 1024.0f));
        CompressedKrt0->Translation.Y = static_cast<int16_t>((Krts.Translation.Y * 1024.0f));
        CompressedKrt0->Translation.Z = static_cast<int16_t>((Krts.Translation.Z * 1024.0f));
    }
}

void CClientIFP::ReadKrt0FramesAsCompressed(std::unique_ptr<CAnimBlendSequence>& pAnimationSequence, const std::int32_t& cFrames)
{
    for (std::int32_t FrameIndex = 0; FrameIndex < cFrames; FrameIndex++)
    {
        SCompressed_KRT0* CompressedKrt0 = static_cast<SCompressed_KRT0*>(pAnimationSequence->GetKeyFrame(FrameIndex, sizeof(SCompressed_KRT0)));
        SKrt0             Krt0;
        ReadBuffer<SKrt0>(&Krt0);

        CompressedKrt0->Rotation.X = static_cast<int16_t>(((-Krt0.Rotation.X) * 4096.0f));
        CompressedKrt0->Rotation.Y = static_cast<int16_t>(((-Krt0.Rotation.Y) * 4096.0f));
        CompressedKrt0->Rotation.Z = static_cast<int16_t>(((-Krt0.Rotation.Z) * 4096.0f));
        CompressedKrt0->Rotation.W = static_cast<int16_t>((Krt0.Rotation.W * 4096.0f));
        CompressedKrt0->Time = static_cast<int16_t>((Krt0.Time * 60.0f + 0.5f));
        CompressedKrt0->Translation.X = static_cast<int16_t>((Krt0.Translation.X * 1024.0f));
        CompressedKrt0->Translation.Y = static_cast<int16_t>((Krt0.Translation.Y * 1024.0f));
        CompressedKrt0->Translation.Z = static_cast<int16_t>((Krt0.Translation.Z * 1024.0f));
    }
}

void CClientIFP::ReadKr00FramesAsCompressed(std::unique_ptr<CAnimBlendSequence>& pAnimationSequence, const std::int32_t& cFrames)
{
    for (std::int32_t FrameIndex = 0; FrameIndex < cFrames; FrameIndex++)
    {
        SCompressed_KR00* CompressedKr00 = static_cast<SCompressed_KR00*>(pAnimationSequence->GetKeyFrame(FrameIndex, sizeof(SCompressed_KR00)));
        SKr00             Kr00;
        ReadBuffer<SKr00>(&Kr00);

        CompressedKr00->Rotation.X = static_cast<int16_t>(((-Kr00.Rotation.X) * 4096.0f));
        CompressedKr00->Rotation.Y = static_cast<int16_t>(((-Kr00.Rotation.Y) * 4096.0f));
        CompressedKr00->Rotation.Z = static_cast<int16_t>(((-Kr00.Rotation.Z) * 4096.0f));
        CompressedKr00->Rotation.W = static_cast<int16_t>((Kr00.Rotation.W * 4096.0f));
        CompressedKr00->Time = static_cast<int16_t>((Kr00.Time * 60.0f + 0.5f));
    }
}

size_t CClientIFP::GetSizeOfCompressedFrame(eFrameType iFrameType)
{
    switch (iFrameType)
    {
        case eFrameType::KRTS:
        {
            return sizeof(SCompressed_KRT0);
        }
        case eFrameType::KRT0:
        {
            return sizeof(SCompressed_KRT0);
        }
        case eFrameType::KR00:
        {
            return sizeof(SCompressed_KR00);
        }
        case eFrameType::KR00_COMPRESSED:
        {
            return sizeof(SCompressed_KR00);
        }
        case eFrameType::KRT0_COMPRESSED:
        {
            return sizeof(SCompressed_KRT0);
        }
    }
    return 0;
}

size_t CClientIFP::GetSourceFrameDataSize(eFrameType iFrameType)
{
    switch (iFrameType)
    {
        case eFrameType::KRTS:
        {
            return sizeof(SKrts);
        }
        case eFrameType::KRT0:
        {
            return sizeof(SKrt0);
        }
        case eFrameType::KR00:
        {
            return sizeof(SKr00);
        }
        case eFrameType::KR00_COMPRESSED:
        {
            return sizeof(SCompressed_KR00);
        }
        case eFrameType::KRT0_COMPRESSED:
        {
            return sizeof(SCompressed_KRT0);
        }
    }
    return 0;
}

void CClientIFP::InitializeAnimationHierarchy(std::unique_ptr<CAnimBlendHierarchy>& pAnimationHierarchy, const SString& strAnimationName,
                                              const std::int32_t& iSequences)
{
    MtaGame::IfpRead::SequenceCapacity(iSequences);
    if (!pAnimationHierarchy)
        throw std::runtime_error("IFP backend returned no hierarchy");
    pAnimationHierarchy->Initialize();
    pAnimationHierarchy->SetName(strAnimationName);
    pAnimationHierarchy->SetNumSequences(static_cast<unsigned short>(iSequences));
    pAnimationHierarchy->SetAnimationBlockID(-1);
    pAnimationHierarchy->SetRunningCompressed(m_kbAllKeyFramesCompressed);
}

void CClientIFP::InitializeAnimationSequence(std::unique_ptr<CAnimBlendSequence>& pAnimationSequence, const SString& strName, const std::int32_t& iBoneID)
{
    pAnimationSequence->Initialize();
    pAnimationSequence->SetName(strName);
    pAnimationSequence->SetBoneTag(iBoneID);
}

void CClientIFP::PreProcessAnimationHierarchy(std::unique_ptr<CAnimBlendHierarchy>& pAnimationHierarchy)
{
    if (!pAnimationHierarchy->IsRunningCompressed())
    {
        pAnimationHierarchy->RemoveQuaternionFlips();
        pAnimationHierarchy->CalculateTotalTime();
    }
}

void CClientIFP::MoveSequencesWithDummies(SAnimation& animation, SequenceMapType& mapOfSequences, std::bitset<64>& outAnimatedBonesMask)
{
    for (size_t SequenceIndex = 0; SequenceIndex < m_kcIFPSequences; SequenceIndex++)
    {
        SString BoneName = m_karrstrBoneNames[SequenceIndex];
        DWORD   BoneID = m_karruBoneIds[SequenceIndex];

        auto pAnimationSequence = m_pAnimManager->GetCustomAnimBlendSequenceAt(animation.pSequences, static_cast<DWORD>(SequenceIndex));
        if (!pAnimationSequence)
            throw std::runtime_error("IFP backend returned no owned sequence row");
        auto it = mapOfSequences.find(BoneID);
        if (it != mapOfSequences.end())
        {
            // The detached guard transfers the frame pointer before deleting
            // its source interface. Failed copies retain one cleanup owner.
            it->second.TransferTo(*pAnimationSequence);
            outAnimatedBonesMask.set(SequenceIndex);
        }
        else
        {
            InsertAnimationDummySequence(pAnimationSequence, BoneName, BoneID);
        }
    }
}

void CClientIFP::AllocateSequencesMemory(SAnimation& animation)
{
    const auto capacity = MtaGame::IfpRead::SequenceCapacity(animation.pHierarchy->GetNumSequences());
    auto*      sequences = m_pAnimManager->AllocateCustomAnimSequences(capacity);
    if (!sequences)
        throw std::bad_alloc();
    animation.pSequences = sequences;
    animation.sequenceCapacity = capacity;
    // Populate through the original owned array. The hierarchy edge is added
    // only after all row/frame construction and transfers have completed.
}

void CClientIFP::AttachSequencesMemory(SAnimation& animation)
{
    animation.pHierarchy->SetSequences(animation.pSequences);
    animation.sequencesAttached = true;
}

CClientIFP::eFrameType CClientIFP::GetFrameTypeFromFourCC(const char* szFourCC)
{
    if (strncmp(szFourCC, "KRTS", 4) == 0)
    {
        return eFrameType::KRTS;
    }
    else if (strncmp(szFourCC, "KRT0", 4) == 0)
    {
        return eFrameType::KRT0;
    }
    else if (strncmp(szFourCC, "KR00", 4) == 0)
    {
        return eFrameType::KR00;
    }

    return eFrameType::UNKNOWN_FRAME;
}

void CClientIFP::InsertAnimationDummySequence(std::unique_ptr<CAnimBlendSequence>& pAnimationSequence, const SString& BoneName, const DWORD& dwBoneID)
{
    InitializeAnimationSequence(pAnimationSequence, BoneName, dwBoneID);

    bool bRootBone = dwBoneID == eBoneType::NORMAL;
    bool bHasTranslationValues = false;

    // We only need 1 dummy key frame to make the animation work
    const size_t cKeyFrames = 1;
    // KR00 is child key frame and KRT0 is Root key frame
    size_t FrameSize = sizeof(SCompressed_KR00);

    if (bRootBone)
    {
        // This key frame will have translation values.
        FrameSize = sizeof(SCompressed_KRT0);
        bHasTranslationValues = true;
    }

    const size_t FramesDataSizeInBytes = FrameSize * cKeyFrames;
    BYTE*        pKeyFrames = m_pAnimManager->AllocateKeyFramesMemory(FramesDataSizeInBytes);
    if (!pKeyFrames)
        return;

    MtaGame::IfpStorage::AttachFrames(*m_pAnimManager, *pAnimationSequence, cKeyFrames, bHasTranslationValues, m_kbAllKeyFramesCompressed, pKeyFrames);
    CopyDummyKeyFrameByBoneID(pKeyFrames, dwBoneID);
}

void CClientIFP::CopyDummyKeyFrameByBoneID(BYTE* pKeyFrames, DWORD dwBoneID)
{
    /* FrameData structure:
    *
    struct KeyFrame
    {
        short quaternionX;
        short quaternionY;
        short quaternionZ;
        short quaternionW;
        short time;
    };

    struct KeyFrameCompressed : KeyFrame
    {
        short translationX;
        short translationY;
        short translationZ;
    };
    */

    switch (dwBoneID)
    {
        case eBoneType::NORMAL:  // Normal or Root, both are same
        {
            // This is a root frame. It contains translation as well, but it's compressed just like quaternion
            BYTE FrameData[16] = {0x1F, 0x00, 0x00, 0x00, 0x53, 0x0B, 0x4D, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::PELVIS:
        {
            BYTE FrameData[10] = {0xB0, 0xF7, 0xB0, 0xF7, 0x55, 0xF8, 0xAB, 0x07, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::SPINE:
        {
            BYTE FrameData[10] = {0x0E, 0x00, 0x15, 0x00, 0xBE, 0xFF, 0xFF, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::SPINE1:
        {
            BYTE FrameData[10] = {0x29, 0x00, 0xD9, 0x00, 0xB5, 0x00, 0xF5, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::NECK:
        {
            BYTE FrameData[10] = {0x86, 0xFF, 0xB6, 0xFF, 0x12, 0x02, 0xDA, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::HEAD:
        {
            BYTE FrameData[10] = {0xFA, 0x00, 0x0C, 0x01, 0x96, 0xFE, 0xDF, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::JAW:
        {
            BYTE FrameData[10] = {0x00, 0x00, 0x00, 0x00, 0x2B, 0x0D, 0x16, 0x09, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_BROW:
        {
            BYTE FrameData[10] = {0xC4, 0x00, 0xFF, 0xFE, 0x47, 0x09, 0xF8, 0x0C, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_BROW:
        {
            BYTE FrameData[10] = {0xA2, 0xFF, 0xF8, 0x00, 0x4F, 0x09, 0xF8, 0x0C, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_CLAVICLE:
        {
            BYTE FrameData[10] = {0x7E, 0xF5, 0x82, 0x02, 0x37, 0xF4, 0x6A, 0xFF, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_UPPER_ARM:
        {
            BYTE FrameData[10] = {0xA8, 0x02, 0x6D, 0x09, 0x1F, 0xFD, 0x51, 0x0C, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_FORE_ARM:
        {
            BYTE FrameData[10] = {0x00, 0x00, 0x00, 0x00, 0x3A, 0xFE, 0xE6, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_HAND:
        {
            BYTE FrameData[10] = {0x9D, 0xF5, 0xBA, 0xFF, 0xEA, 0xFF, 0x29, 0x0C, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_FINGER:
        {
            BYTE FrameData[10] = {0xFF, 0xFF, 0x00, 0x00, 0xD8, 0x04, 0x3F, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_FINGER_01:
        {
            BYTE FrameData[10] = {0x00, 0x00, 0x00, 0x00, 0x54, 0x06, 0xB1, 0x0E, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_CLAVICLE:
        {
            BYTE FrameData[10] = {0x0B, 0x0A, 0x3D, 0xFE, 0xBB, 0xF3, 0xCF, 0xFE, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_UPPER_ARM:
        {
            BYTE FrameData[10] = {0xF7, 0xFD, 0xED, 0xF6, 0xEB, 0xFD, 0xD9, 0x0C, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_FORE_ARM:
        {
            BYTE FrameData[10] = {0x00, 0x00, 0x00, 0x00, 0x42, 0xFF, 0xFB, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_HAND:
        {
            BYTE FrameData[10] = {0xE9, 0x0B, 0x94, 0x00, 0x25, 0x02, 0x72, 0x0A, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_FINGER:
        {
            BYTE FrameData[10] = {0x37, 0x00, 0xCB, 0xFF, 0x10, 0x09, 0x2E, 0x0D, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_FINGER_01:
        {
            BYTE FrameData[10] = {0x00, 0x00, 0x00, 0x00, 0x60, 0x06, 0xAC, 0x0E, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_BREAST:
        {
            BYTE FrameData[10] = {0xC5, 0x01, 0x2B, 0x01, 0x53, 0x0A, 0x09, 0x0C, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_BREAST:
        {
            BYTE FrameData[10] = {0x2F, 0xFF, 0xA5, 0xFF, 0xBA, 0x0A, 0xD6, 0x0B, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::BELLY:
        {
            BYTE FrameData[10] = {0x00, 0x00, 0x00, 0x00, 0x20, 0x0B, 0x7F, 0x0B, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_THIGH:
        {
            BYTE FrameData[10] = {0x23, 0xFE, 0x44, 0xF0, 0x19, 0xFE, 0x25, 0x01, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_CALF:
        {
            BYTE FrameData[10] = {0x00, 0x00, 0x00, 0x00, 0x1E, 0xFC, 0x85, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_FOOT:
        {
            BYTE FrameData[10] = {0xBB, 0xFE, 0x3E, 0xFF, 0xD2, 0x01, 0xD3, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::L_TOE_0:
        {
            BYTE FrameData[10] = {0x00, 0x00, 0x01, 0x00, 0x8D, 0x0B, 0x12, 0x0B, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_THIGH:
        {
            BYTE FrameData[10] = {0x0F, 0xFF, 0x19, 0xF0, 0x44, 0x01, 0xBB, 0x00, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_CALF:
        {
            BYTE FrameData[10] = {0x00, 0x00, 0x00, 0x00, 0x64, 0xFD, 0xC9, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_FOOT:
        {
            BYTE FrameData[10] = {0x11, 0x01, 0x9F, 0xFF, 0x9E, 0x01, 0xE0, 0x0F, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        case eBoneType::R_TOE_0:
        {
            BYTE FrameData[10] = {0x00, 0x00, 0x00, 0x00, 0x50, 0x0B, 0x4F, 0x0B, 0x00, 0x00};
            memcpy(pKeyFrames, FrameData, sizeof(FrameData));
            break;
        }
        default:
        {
        }
    }
}

SString CClientIFP::ConvertStringToKey(const SString& strBoneName, bool isANPK)
{
    SString ConvertedString = strBoneName;

    // Fix for some cutscene bones (like cssuitcase:Pelvis etc.)
    if (isANPK)
    {
        std::size_t pos = ConvertedString.find(":");
        if (pos != std::string::npos)
            ConvertedString = ConvertedString.substr(pos + 1);
    }

    ConvertedString = ConvertedString.ToLower();

    // Remove whitespaces
    ConvertedString.erase(std::remove(ConvertedString.begin(), ConvertedString.end(), ' '), ConvertedString.end());
    return ConvertedString;
}

constexpr void CClientIFP::RoundSize(std::uint32_t& u32Size)
{
    u32Size = MtaGame::IfpRead::Padded(u32Size);
}

constexpr bool CClientIFP::IsKeyFramesTypeRoot(eFrameType iFrameType)
{
    switch (iFrameType)
    {
        case eFrameType::KR00:
        {
            return false;
        }
        case eFrameType::KR00_COMPRESSED:
        {
            return false;
        }
    }
    return true;
}

std::int32_t CClientIFP::GetBoneIDFromName(const SString& strBoneName)
{
    if (strBoneName == "root")
        return eBoneType::NORMAL;
    if (strBoneName == "normal")
        return eBoneType::NORMAL;

    if (strBoneName == "pelvis")
        return eBoneType::PELVIS;
    if (strBoneName == "spine")
        return eBoneType::SPINE;
    if (strBoneName == "spine1")
        return eBoneType::SPINE1;
    if (strBoneName == "neck")
        return eBoneType::NECK;
    if (strBoneName == "head")
        return eBoneType::HEAD;
    if (strBoneName == "jaw")
        return eBoneType::JAW;
    if (strBoneName == "lbrow")
        return eBoneType::L_BROW;
    if (strBoneName == "rbrow")
        return eBoneType::R_BROW;
    if (strBoneName == "bip01lclavicle")
        return eBoneType::L_CLAVICLE;
    if (strBoneName == "lupperarm")
        return eBoneType::L_UPPER_ARM;
    if (strBoneName == "lforearm")
        return eBoneType::L_FORE_ARM;
    if (strBoneName == "lhand")
        return eBoneType::L_HAND;

    if (strBoneName == "lfingers")
        return eBoneType::L_FINGER;
    if (strBoneName == "lfinger")
        return eBoneType::L_FINGER;

    if (strBoneName == "lfinger01")
        return eBoneType::L_FINGER_01;
    if (strBoneName == "bip01rclavicle")
        return eBoneType::R_CLAVICLE;
    if (strBoneName == "rupperarm")
        return eBoneType::R_UPPER_ARM;
    if (strBoneName == "rforearm")
        return eBoneType::R_FORE_ARM;
    if (strBoneName == "rhand")
        return eBoneType::R_HAND;

    if (strBoneName == "rfingers")
        return eBoneType::R_FINGER;
    if (strBoneName == "rfinger")
        return eBoneType::R_FINGER;

    if (strBoneName == "rfinger01")
        return eBoneType::R_FINGER_01;
    if (strBoneName == "lbreast")
        return eBoneType::L_BREAST;
    if (strBoneName == "rbreast")
        return eBoneType::R_BREAST;
    if (strBoneName == "belly")
        return eBoneType::BELLY;
    if (strBoneName == "lthigh")
        return eBoneType::L_THIGH;
    if (strBoneName == "lcalf")
        return eBoneType::L_CALF;
    if (strBoneName == "lfoot")
        return eBoneType::L_FOOT;
    if (strBoneName == "ltoe0")
        return eBoneType::L_TOE_0;
    if (strBoneName == "rthigh")
        return eBoneType::R_THIGH;
    if (strBoneName == "rcalf")
        return eBoneType::R_CALF;
    if (strBoneName == "rfoot")
        return eBoneType::R_FOOT;
    if (strBoneName == "rtoe0")
        return eBoneType::R_TOE_0;

    // Cutscene bones
    if (strBoneName == "rthumb1")
        return eBoneType::R_Thumb1;
    if (strBoneName == "rthumb2")
        return eBoneType::R_Thumb2;
    if (strBoneName == "lthumb1")
        return eBoneType::L_Thumb1;
    if (strBoneName == "lthumb2")
        return eBoneType::L_Thumb2;
    if (strBoneName == "llip11")
        return eBoneType::LLIP11;
    if (strBoneName == "jaw22")
        return eBoneType::JAW22;
    if (strBoneName == "headnub")
        return eBoneType::HEADNUB;
    if (strBoneName == "lfinger0nub")
        return eBoneType::L_Finger0Nub;
    if (strBoneName == "rfinger0nub")
        return eBoneType::R_Finger0Nub;
    if (strBoneName == "ltoe0nub")
        return eBoneType::L_Toe0Nub;
    if (strBoneName == "rtoe0nub")
        return eBoneType::R_Toe0Nub;
    if (strBoneName == "rbrow1")
        return eBoneType::R_BROW1;
    if (strBoneName == "rbrow2")
        return eBoneType::R_BROW2;
    if (strBoneName == "lbrow1")
        return eBoneType::L_BROW1;
    if (strBoneName == "lbrow2")
        return eBoneType::L_BROW2;
    if (strBoneName == "rlid")
        return eBoneType::R_LID;
    if (strBoneName == "llid")
        return eBoneType::L_LID;
    if (strBoneName == "rtlip3")
        return eBoneType::R_TLIP3;
    if (strBoneName == "ltlip3")
        return eBoneType::L_TLIP3;
    if (strBoneName == "rtlip1")
        return eBoneType::R_TLIP1;
    if (strBoneName == "rtlip2")
        return eBoneType::R_TLIP2;
    if (strBoneName == "ltlip1")
        return eBoneType::L_TLIP1;
    if (strBoneName == "ltlip2")
        return eBoneType::L_TLIP2;
    if (strBoneName == "rcorner")
        return eBoneType::R_CORNER;
    if (strBoneName == "lcorner")
        return eBoneType::L_CORNER;
    if (strBoneName == "jaw1")
        return eBoneType::JAW1;
    if (strBoneName == "jaw2")
        return eBoneType::JAW2;
    if (strBoneName == "llip1")
        return eBoneType::LLIP1;
    if (strBoneName == "reye")
        return eBoneType::R_EYE;
    if (strBoneName == "leye")
        return eBoneType::L_EYE;
    if (strBoneName == "rcheek")
        return eBoneType::R_CHEEK;
    if (strBoneName == "lcheek")
        return eBoneType::L_CHEEK;

    // for GTA 3
    if (strBoneName == "player")
        return eBoneType::NORMAL;

    if (strBoneName == "swaist")
        return eBoneType::PELVIS;
    if (strBoneName == "smid")
        return eBoneType::SPINE;
    if (strBoneName == "storso")
        return eBoneType::SPINE1;
    if (strBoneName == "shead")
        return eBoneType::HEAD;

    if (strBoneName == "supperarml")
        return eBoneType::L_UPPER_ARM;
    if (strBoneName == "slowerarml")
        return eBoneType::L_FORE_ARM;
    if (strBoneName == "supperarmr")
        return eBoneType::R_UPPER_ARM;
    if (strBoneName == "slowerarmr")
        return eBoneType::R_FORE_ARM;

    if (strBoneName == "srhand")
        return eBoneType::R_HAND;
    if (strBoneName == "slhand")
        return eBoneType::L_HAND;

    if (strBoneName == "supperlegr")
        return eBoneType::R_THIGH;
    if (strBoneName == "slowerlegr")
        return eBoneType::R_CALF;
    if (strBoneName == "sfootr")
        return eBoneType::R_FOOT;

    if (strBoneName == "supperlegl")
        return eBoneType::L_THIGH;
    if (strBoneName == "slowerlegl")
        return eBoneType::L_CALF;
    if (strBoneName == "sfootl")
        return eBoneType::L_FOOT;

    return eBoneType::UNKNOWN;
}

SString CClientIFP::GetCorrectBoneNameFromID(const std::int32_t& iBoneID)
{
    if (iBoneID == eBoneType::NORMAL)
        return "Normal";

    if (iBoneID == eBoneType::PELVIS)
        return "Pelvis";
    if (iBoneID == eBoneType::SPINE)
        return "Spine";
    if (iBoneID == eBoneType::SPINE1)
        return "Spine1";
    if (iBoneID == eBoneType::NECK)
        return "Neck";
    if (iBoneID == eBoneType::HEAD)
        return "Head";
    if (iBoneID == eBoneType::JAW)
        return "Jaw";
    if (iBoneID == eBoneType::L_BROW)
        return "L Brow";
    if (iBoneID == eBoneType::R_BROW)
        return "R Brow";
    if (iBoneID == eBoneType::L_CLAVICLE)
        return "Bip01 L Clavicle";
    if (iBoneID == eBoneType::L_UPPER_ARM)
        return "L UpperArm";
    if (iBoneID == eBoneType::L_FORE_ARM)
        return "L ForeArm";
    if (iBoneID == eBoneType::L_HAND)
        return "L Hand";

    if (iBoneID == eBoneType::L_FINGER)
        return "L Finger";

    if (iBoneID == eBoneType::L_FINGER_01)
        return "L Finger01";
    if (iBoneID == eBoneType::R_CLAVICLE)
        return "Bip01 R Clavicle";
    if (iBoneID == eBoneType::R_UPPER_ARM)
        return "R UpperArm";
    if (iBoneID == eBoneType::R_FORE_ARM)
        return "R ForeArm";
    if (iBoneID == eBoneType::R_HAND)
        return "R Hand";

    if (iBoneID == eBoneType::R_FINGER)
        return "R Finger";

    if (iBoneID == eBoneType::R_FINGER_01)
        return "R Finger01";
    if (iBoneID == eBoneType::L_BREAST)
        return "L breast";
    if (iBoneID == eBoneType::R_BREAST)
        return "R breast";
    if (iBoneID == eBoneType::BELLY)
        return "Belly";
    if (iBoneID == eBoneType::L_THIGH)
        return "L Thigh";
    if (iBoneID == eBoneType::L_CALF)
        return "L Calf";
    if (iBoneID == eBoneType::L_FOOT)
        return "L Foot";
    if (iBoneID == eBoneType::L_TOE_0)
        return "L Toe0";
    if (iBoneID == eBoneType::R_THIGH)
        return "R Thigh";
    if (iBoneID == eBoneType::R_CALF)
        return "R Calf";
    if (iBoneID == eBoneType::R_FOOT)
        return "R Foot";
    if (iBoneID == eBoneType::R_TOE_0)
        return "R Toe0";

    // Cutscene bones
    if (iBoneID == eBoneType::R_Thumb1)
        return "RThumb1";
    if (iBoneID == eBoneType::R_Thumb2)
        return "RThumb2";
    if (iBoneID == eBoneType::LLIP11)
        return "llip11";
    if (iBoneID == eBoneType::L_Thumb1)
        return "LThumb1";
    if (iBoneID == eBoneType::L_Thumb2)
        return "LThumb2";
    if (iBoneID == eBoneType::JAW22)
        return "jaw22";
    if (iBoneID == eBoneType::HEADNUB)
        return "HeadNub";
    if (iBoneID == eBoneType::L_Finger0Nub)
        return "L Finger0Nub";
    if (iBoneID == eBoneType::R_Finger0Nub)
        return "R Finger0Nub";
    if (iBoneID == eBoneType::L_Toe0Nub)
        return "L Toe0Nub";
    if (iBoneID == eBoneType::R_Toe0Nub)
        return "R Toe0Nub";
    if (iBoneID == eBoneType::R_BROW1)
        return "rbrow1";
    if (iBoneID == eBoneType::R_BROW2)
        return "rbrow2";
    if (iBoneID == eBoneType::L_BROW2)
        return "lbrow2";
    if (iBoneID == eBoneType::L_BROW1)
        return "lbrow1";
    if (iBoneID == eBoneType::R_LID)
        return "rlid";
    if (iBoneID == eBoneType::L_LID)
        return "llid";
    if (iBoneID == eBoneType::R_TLIP3)
        return "rtlip3";
    if (iBoneID == eBoneType::L_TLIP3)
        return "ltlip3";
    if (iBoneID == eBoneType::R_TLIP1)
        return "rtlip1";
    if (iBoneID == eBoneType::R_TLIP2)
        return "rtlip2";
    if (iBoneID == eBoneType::L_TLIP1)
        return "ltlip1";
    if (iBoneID == eBoneType::L_TLIP2)
        return "ltlip2";
    if (iBoneID == eBoneType::R_CORNER)
        return "rcorner";
    if (iBoneID == eBoneType::L_CORNER)
        return "lcorner";
    if (iBoneID == eBoneType::JAW1)
        return "jaw1";
    if (iBoneID == eBoneType::JAW2)
        return "jaw2";
    if (iBoneID == eBoneType::LLIP1)
        return "llip1";
    if (iBoneID == eBoneType::R_EYE)
        return "reye";
    if (iBoneID == eBoneType::L_EYE)
        return "leye";
    if (iBoneID == eBoneType::R_CHEEK)
        return "rcheek";
    if (iBoneID == eBoneType::L_CHEEK)
        return "lcheek";

    return "";
}

SString CClientIFP::GetCorrectBoneNameFromName(const SString& strBoneName)
{
    if (strBoneName == "root")
        return "Normal";
    if (strBoneName == "normal")
        return "Normal";

    if (strBoneName == "pelvis")
        return "Pelvis";
    if (strBoneName == "spine")
        return "Spine";
    if (strBoneName == "spine1")
        return "Spine1";
    if (strBoneName == "neck")
        return "Neck";
    if (strBoneName == "head")
        return "Head";
    if (strBoneName == "jaw")
        return "Jaw";
    if (strBoneName == "lbrow")
        return "L Brow";
    if (strBoneName == "rbrow")
        return "R Brow";
    if (strBoneName == "bip01lclavicle")
        return "Bip01 L Clavicle";
    if (strBoneName == "lupperarm")
        return "L UpperArm";
    if (strBoneName == "lforearm")
        return "L ForeArm";
    if (strBoneName == "lhand")
        return "L Hand";

    if (strBoneName == "lfingers")
        return "L Finger";
    if (strBoneName == "lfinger")
        return "L Finger";

    if (strBoneName == "lfinger01")
        return "L Finger01";
    if (strBoneName == "bip01rclavicle")
        return "Bip01 R Clavicle";
    if (strBoneName == "rupperarm")
        return "R UpperArm";
    if (strBoneName == "rforearm")
        return "R ForeArm";
    if (strBoneName == "rhand")
        return "R Hand";

    if (strBoneName == "rfingers")
        return "R Finger";
    if (strBoneName == "rfinger")
        return "R Finger";

    if (strBoneName == "rfinger01")
        return "R Finger01";
    if (strBoneName == "lbreast")
        return "L Breast";
    if (strBoneName == "rbreast")
        return "R Breast";
    if (strBoneName == "belly")
        return "Belly";
    if (strBoneName == "lthigh")
        return "L Thigh";
    if (strBoneName == "lcalf")
        return "L Calf";
    if (strBoneName == "lfoot")
        return "L Foot";
    if (strBoneName == "ltoe0")
        return "L Toe0";
    if (strBoneName == "rthigh")
        return "R Thigh";
    if (strBoneName == "rcalf")
        return "R Calf";
    if (strBoneName == "rfoot")
        return "R Foot";
    if (strBoneName == "rtoe0")
        return "R Toe0";

    // Cutscene bones
    if (strBoneName == "rthumb1")
        return "RThumb1";
    if (strBoneName == "rthumb2")
        return "RThumb2";
    if (strBoneName == "llip11")
        return "llip11";
    if (strBoneName == "lthumb1")
        return "LThumb1";
    if (strBoneName == "lthumb2")
        return "LThumb2";
    if (strBoneName == "jaw22")
        return "jaw22";
    if (strBoneName == "headnub")
        return "HeadNub";
    if (strBoneName == "lfinger0nub")
        return "L Finger0Nub";
    if (strBoneName == "rfinger0nub")
        return "R Finger0Nub";
    if (strBoneName == "ltoe0nub")
        return "L Toe0Nub";
    if (strBoneName == "rtoe0nub")
        return "R Toe0Nub";
    if (strBoneName == "rbrow1")
        return "rbrow1";
    if (strBoneName == "rbrow2")
        return "rbrow2";
    if (strBoneName == "lbrow1")
        return "lbrow1";
    if (strBoneName == "lbrow2")
        return "lbrow2";
    if (strBoneName == "rlid")
        return "rlid";
    if (strBoneName == "llid")
        return "llid";
    if (strBoneName == "rtlip3")
        return "rtlip3";
    if (strBoneName == "ltlip3")
        return "ltlip3";
    if (strBoneName == "rtlip1")
        return "rtlip1";
    if (strBoneName == "rtlip2")
        return "rtlip2";
    if (strBoneName == "ltlip1")
        return "ltlip1";
    if (strBoneName == "ltlip2")
        return "ltlip2";
    if (strBoneName == "rcorner")
        return "rcorner";
    if (strBoneName == "lcorner")
        return "lcorner";
    if (strBoneName == "jaw1")
        return "jaw1";
    if (strBoneName == "jaw2")
        return "jaw2";
    if (strBoneName == "llip1")
        return "llip1";
    if (strBoneName == "reye")
        return "reye";
    if (strBoneName == "leye")
        return "leye";
    if (strBoneName == "rcheek")
        return "rcheek";
    if (strBoneName == "lcheek")
        return "lcheek";

    // For GTA 3
    if (strBoneName == "player")
        return "Normal";
    if (strBoneName == "swaist")
        return "Pelvis";
    if (strBoneName == "smid")
        return "Spine";
    if (strBoneName == "storso")
        return "Spine1";
    if (strBoneName == "shead")
        return "Head";

    if (strBoneName == "supperarml")
        return "L UpperArm";
    if (strBoneName == "slowerarml")
        return "L ForeArm";
    if (strBoneName == "supperarmr")
        return "R UpperArm";
    if (strBoneName == "slowerarmr")
        return "R ForeArm";

    if (strBoneName == "srhand")
        return "R Hand";
    if (strBoneName == "slhand")
        return "L Hand";
    if (strBoneName == "supperlegr")
        return "R Thigh";
    if (strBoneName == "slowerlegr")
        return "R Calf";
    if (strBoneName == "sfootr")
        return "R Foot";
    if (strBoneName == "supperlegl")
        return "L Thigh";
    if (strBoneName == "slowerlegl")
        return "L Calf";
    if (strBoneName == "sfootl")
        return "L Foot";

    return strBoneName;
}

CAnimBlendHierarchySAInterface* CClientIFP::GetAnimationHierarchy(const SString& strAnimationName)
{
    const unsigned int uiAnimationNameHash = HashString(strAnimationName.ToLower());
    auto               it = std::find_if(m_pIFPAnimations->vecAnimations.begin(), m_pIFPAnimations->vecAnimations.end(),
                                         [&uiAnimationNameHash](SAnimation const& Animation) { return Animation.uiNameHash == uiAnimationNameHash; });
    if (it != m_pIFPAnimations->vecAnimations.end())
    {
        return it->pHierarchy->GetInterface();
    }
    return nullptr;
}

std::bitset<64> CClientIFP::GetAnimatedBonesMask(const SString& strAnimationName)
{
    const unsigned int uiAnimationNameHash = HashString(strAnimationName.ToLower());
    auto               it = std::find_if(m_pIFPAnimations->vecAnimations.begin(), m_pIFPAnimations->vecAnimations.end(),
                                         [&uiAnimationNameHash](SAnimation const& Animation) { return Animation.uiNameHash == uiAnimationNameHash; });
    if (it != m_pIFPAnimations->vecAnimations.end())
    {
        return it->AnimatedBonesMask;
    }
    return {};
}
