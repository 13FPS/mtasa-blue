/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reject unqualified methods in the actual SDK manager owner fixture
 ****************************************************************************/
#pragma once
// Generated from the real common interface. A complete test leaf provides no
// successful factory, initialization, hierarchy, association or ref service.
class AnimationManagerOwnerSdk final : public mta::game_de::CAnimManagerDE
{
public:
    AnimationManagerOwnerSdk(mta::game_de::NativeAnimationManagerAccess& access, std::uint64_t generation) : CAnimManagerDE(access, generation) {}
    [[noreturn]] static void Unsupported()
    {
        std::fputs("Unexpected unqualified animation manager SDK call\n", stderr);
        std::abort();
    }
    void                                 Initialize() override { Unsupported(); }
    void                                 Shutdown() override { Unsupported(); }
    int                                  GetNumAnimAssocDefinitions() override { Unsupported(); }
    std::unique_ptr<CAnimBlendHierarchy> GetAnimation(int) override { Unsupported(); }
    std::unique_ptr<CAnimBlendHierarchy> GetAnimation(const char*, std::unique_ptr<CAnimBlock>&) override { Unsupported(); }
    std::unique_ptr<CAnimBlendHierarchy> GetAnimation(unsigned int, std::unique_ptr<CAnimBlock>&) override { Unsupported(); }
    std::unique_ptr<CAnimBlock>          GetAnimationBlock(int) override { Unsupported(); }
    std::unique_ptr<CAnimBlock>          GetAnimationBlock(const char*) override { Unsupported(); }
    int                                  RegisterAnimBlock(const char*) override { Unsupported(); }
    AnimAssocGroup_type                  GetAnimBlendAssoc(AssocGroupId) const override { Unsupported(); }
    AssocGroupId                         GetFirstAssocGroup(const char*) override { Unsupported(); }
    const char*                          GetAnimGroupName(AssocGroupId) override { Unsupported(); }
    const char*                          GetAnimBlockName(AssocGroupId) override { Unsupported(); }
    AnimBlendAssoc_type                  CreateAnimAssociation(AssocGroupId, AnimationId) override { Unsupported(); }
    StaticAssocIntface_type              GetAnimStaticAssociation(eAnimGroup, eAnimID) const override { Unsupported(); }
    AnimBlendAssoc_type                  GetAnimAssociation(AssocGroupId, const char*) override { Unsupported(); }
    AnimBlendAssoc_type                  AddAnimation(RpClump*, AssocGroupId, AnimationId) override { Unsupported(); }
    AnimBlendAssoc_type                  AddAnimation(RpClump*, CAnimBlendHierarchy*, int) override { Unsupported(); }
    AnimBlendAssoc_type                  AddAnimationAndSync(RpClump*, CAnimBlendAssociation*, AssocGroupId, AnimationId) override { Unsupported(); }
    AnimBlendAssoc_type                  BlendAnimation(RpClump*, AssocGroupId, AnimationId, float) override { Unsupported(); }
    AnimBlendAssoc_type                  BlendAnimation(RpClump*, CAnimBlendHierarchy*, int, float) override { Unsupported(); }
    void                                 AddAnimBlockRef(int) override { Unsupported(); }
    void                                 RemoveAnimBlockRef(int) override { Unsupported(); }
    void                                 RemoveAnimBlockRefWithoutDelete(int) override { Unsupported(); }
    int                                  GetNumRefsToAnimBlock(int) override { Unsupported(); }
    void                                 RemoveAnimBlock(int) override { Unsupported(); }
    AnimAssocDefinition*        AddAnimAssocDefinition(const char*, const char*, AssocGroupId, AnimationId, AnimDescriptor*) override { Unsupported(); }
    void                        ReadAnimAssociationDefinitions() override { Unsupported(); }
    void                        CreateAnimAssocGroups() override { Unsupported(); }
    void                        UncompressAnimation(CAnimBlendHierarchy*) override { Unsupported(); }
    void                        RemoveFromUncompressedCache(CAnimBlendHierarchy*) override { Unsupported(); }
    void                        RemoveFromUncompressedCache(CAnimBlendHierarchySAInterface*) override { Unsupported(); }
    void                        LoadAnimFile(const char*) override { Unsupported(); }
    void                        LoadAnimFile(RwStream*, bool, const char*) override { Unsupported(); }
    void                        LoadAnimFiles() override { Unsupported(); }
    void                        RemoveLastAnimFile() override { Unsupported(); }
    BYTE*                       AllocateKeyFramesMemory(uint32_t) override { Unsupported(); }
    void                        FreeKeyFramesMemory(void*) override { Unsupported(); }
    bool                        HasAnimGroupLoaded(AssocGroupId) override { Unsupported(); }
    AnimBlendAssoc_type         RpAnimBlendClumpGetFirstAssociation(RpClump*) override { Unsupported(); }
    AnimBlendAssoc_type         RpAnimBlendClumpGetAssociation(RpClump*, const char*) override { Unsupported(); }
    AnimBlendAssoc_type         RpAnimBlendClumpGetAssociation(RpClump*, AnimationId) override { Unsupported(); }
    AnimBlendAssoc_type         RpAnimBlendGetNextAssociation(std::unique_ptr<CAnimBlendAssociation>&) override { Unsupported(); }
    int                         RpAnimBlendClumpGetNumAssociations(RpClump*) override { Unsupported(); }
    void                        RpAnimBlendClumpUpdateAnimations(RpClump*, float, bool) override { Unsupported(); }
    AnimBlendAssoc_type         GetAnimBlendAssociation(CAnimBlendAssociationSAInterface*) override { Unsupported(); }
    AnimAssocGroup_type         GetAnimBlendAssocGroup(CAnimBlendAssocGroupSAInterface*) override { Unsupported(); }
    std::unique_ptr<CAnimBlock> GetAnimBlock(CAnimBlockSAInterface*) override { Unsupported(); }
    std::unique_ptr<CAnimBlendHierarchy> GetAnimBlendHierarchy(CAnimBlendHierarchySAInterface*) override { Unsupported(); }
    StaticAssocIntface_type              GetAnimStaticAssociation(CAnimBlendStaticAssociationSAInterface*) override { Unsupported(); }
    std::unique_ptr<CAnimBlendHierarchy> GetCustomAnimBlendHierarchy(CAnimBlendHierarchySAInterface*) override { Unsupported(); }
    std::unique_ptr<CAnimBlendSequence>  GetCustomAnimBlendSequence(CAnimBlendSequenceSAInterface*) override { Unsupported(); }
    std::unique_ptr<CAnimBlendHierarchy> GetCustomAnimBlendHierarchy() override { Unsupported(); }
    std::unique_ptr<CAnimBlendSequence>  GetCustomAnimBlendSequence() override { Unsupported(); }
    void                                 DeleteCustomAnimHierarchyInterface(CAnimBlendHierarchySAInterface*) override { Unsupported(); }
    void                                 DeleteCustomAnimSequenceInterface(CAnimBlendSequenceSAInterface*) override { Unsupported(); }
    bool                                 isGateWayAnimationHierarchy(CAnimBlendHierarchySAInterface*) override { Unsupported(); }
    const char*                          GetGateWayBlockName() const override { Unsupported(); }
    const char*                          GetGateWayAnimationName() const override { Unsupported(); }
    bool                                 IsValidGroup(std::uint32_t) const override { Unsupported(); }
    bool                                 IsValidAnim(std::uint32_t, std::uint32_t) const override { Unsupported(); }
    std::unique_ptr<CAnimBlendSequence>  GetCustomAnimBlendSequenceAt(CAnimBlendSequenceSAInterface*, DWORD) override { Unsupported(); }
    // This observation-only fixture never exercises cleanup. Required lifetime
    // operations must reject visibly, not pretend debt or tickets completed.
    std::unique_ptr<CCustomAnimationCleanup> BeginCustomAnimationCleanup(const SCustomAnimationCleanupRequest&) override { Unsupported(); }
    bool                                     HasPendingCustomAnimationCleanup() const noexcept override { Unsupported(); }
};
