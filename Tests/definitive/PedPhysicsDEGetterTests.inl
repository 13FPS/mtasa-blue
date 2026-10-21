// Only the ordinary-host concrete test subclass implements the mandatory
// native repair hook. These owned fields prove SDK policy/ordering, not engine
// repair qualification or permission to publish a complete production ped.
namespace
{
    bool SameVector(const CVector& a, const CVector& b)
    {
        return std::memcmp(&a, &b, sizeof(a)) == 0;
    }
    bool SameMatrix(const CMatrix& a, const CMatrix& b)
    {
        return SameVector(a.vRight, b.vRight) && SameVector(a.vFront, b.vFront) && SameVector(a.vUp, b.vUp) && SameVector(a.vPos, b.vPos);
    }
    struct PhysicsEvents
    {
        std::array<char, 64> bytes{};
        std::size_t          size = 0;
        void                 Mark(char value) noexcept
        {
            if (size != bytes.size())
                bytes[size++] = value;
        }
        void        Clear() noexcept { size = 0; }
        std::string Text() const { return std::string(bytes.data(), size); }
    };
    class PhysicsOwner final : public NativePedObservationAccess
    {
    public:
        NativePedReferenceToken token;
        PhysicsEvents           events;
        CVector                 position, alternate, linear, angular;
        CVector*                positionAlias = &position;
        CMatrix                 matrix;
        bool                    hasMatrix = true, nullPosition = false;
        ClockResult             positionResult = ClockResult::Ok, matrixResult = ClockResult::Ok, velocityResult = ClockResult::Ok;
        unsigned                positionReads = 0, matrixReads = 0, velocityReads = 0;
        ClockResult             BorrowReferencedPedPosition(const NativePedReferenceToken& requested, CVector*& output) noexcept override
        {
            ++positionReads;
            token = requested;
            events.Mark('P');
            output = nullPosition ? nullptr : positionAlias;
            return positionResult;
        }
        ClockResult ReadReferencedPedTransform(const NativePedReferenceToken& requested, NativePedTransformReader::Value& output) noexcept override
        {
            ++matrixReads;
            token = requested;
            events.Mark('M');
            // A refusing test owner deliberately touches its local output;
            // the real SDK method must not commit it into caller storage.
            output = {};
            output.matrix = hasMatrix ? 1 : 0;
            output.matrixRows = {matrix.vRight, matrix.vFront, matrix.vUp, matrix.vPos};
            return matrixResult;
        }
        ClockResult ReadReferencedPedVelocity(const NativePedReferenceToken& requested, NativePedVelocityKind kind, CVector& output) noexcept override
        {
            ++velocityReads;
            token = requested;
            events.Mark(kind == NativePedVelocityKind::Linear ? 'L' : 'A');
            output = kind == NativePedVelocityKind::Linear ? linear : angular;
            return velocityResult;
        }
    };
    struct PhysicsRepair
    {
        PhysicsOwner&      owner;
        ClockResult        result = ClockResult::Ok;
        CMatrix            requested;
        unsigned           calls = 0;
        bool               apply = true, dropMatrix = false, redirectPosition = false;
        static ClockResult Invoke(void* opaque, const CMatrix& matrix) noexcept
        {
            auto& self = *static_cast<PhysicsRepair*>(opaque);
            ++self.calls;
            self.owner.events.Mark('R');
            self.requested = matrix;
            if (self.apply)
            {
                self.owner.matrix = matrix;
                *self.owner.positionAlias = matrix.vPos;
                self.owner.linear = CVector();
                self.owner.angular = CVector();
            }
            if (self.redirectPosition)
            {
                self.owner.alternate = matrix.vPos;
                self.owner.positionAlias = &self.owner.alternate;
            }
            if (self.dropMatrix)
                self.owner.hasMatrix = false;
            return self.result;
        }
    };

    void RunPhysicsGetterTests()
    {
        Owner                   vitals;
        MetadataOwner           metadata;
        MutationOwner           mutation;
        PhysicsOwner            observation;
        PhysicsRepair           repair{observation};
        NativePedReferenceToken token;
        token.slot = 71;
        token.version = 0x123456789abcdef0;
        token.ownerGeneration = 0x9876543210;
        const auto                 expectedToken = token;
        fixture::PedPhysicsFixture instance(vitals, metadata, mutation, observation, token, &PhysicsRepair::Invoke, &repair);
        CPed&                      ped = instance;
        CPhysical&                 physical = instance;
        CEntity&                   entity = instance;
        token.slot = 2;
        token.version = 3;
        token.ownerGeneration = 4;
        Check(!observation.positionReads && !observation.matrixReads && !observation.velocityReads && !repair.calls,
              "physics layer construction performs no observation, native operation or repair");
        observation.position = CVector(10, 20, 30);
        Check(entity.GetPosition() == &observation.position && observation.events.Text() == "P" && SameToken(observation.token, expectedToken),
              "actual CEntity position returns checked native alias with the inherited immutable token");
        observation.events.Clear();
        observation.linear = CVector(16001, 0, 0);
        CVector output(7, 8, 9);
        Check(physical.GetMoveSpeed(&output) == &output && observation.events.Text() == "LRL" && SameVector(output, CVector()),
              "actual inherited linear getter repairs once, rereads and returns the exact caller output");
        Check(SameVector(repair.requested.vPos, CVector(10, 20, 30)) && SameToken(observation.token, expectedToken),
              "linear repair selects the last successful position, not velocity or caller-mutated token metadata");
        Check(SameVector(repair.requested.vRight, CVector(1, 0, 0)) && SameVector(repair.requested.vFront, CVector(0, 1, 0)) &&
                  SameVector(repair.requested.vUp, CVector(0, 0, 1)),
              "full repair hook receives the actual shared identity-matrix policy");

        observation.events.Clear();
        observation.angular = CVector(4, Float(0x80000000), 6);
        const auto repairsBefore = repair.calls;
        Check(ped.GetTurnSpeed(&output) == &output && observation.events.Text() == "A" && SameVector(output, observation.angular) &&
                  repair.calls == repairsBefore,
              "valid angular getter copies raw signed-zero bits once without extra repair");
        observation.events.Clear();
        observation.angular = CVector(0, Float(0x7fc01234), 0);
        Check(physical.GetTurnSpeed(&output) == &output && observation.events.Text() == "ARA" && SameVector(output, CVector()),
              "NaN angular velocity invokes the same mandatory full-repair policy once");
        Check(SameVector(repair.requested.vPos, CVector(10, 20, 30)), "valid angular observation never replaces the remembered entity position");

        observation.matrix = CMatrix(CVector(40, 50, 60));
        observation.matrix.vRight = CVector(Float(0x7fc04567), 0, 0);
        observation.matrix.vUp = CVector(0, Float(0x7f800000), 0);
        CMatrix matrix;
        observation.events.Clear();
        Check(entity.GetMatrix(&matrix) == &matrix && SameMatrix(matrix, observation.matrix) && observation.events.Text() == "M",
              "actual matrix getter maps all native padded rows into genuine SDK storage and preserves unchecked right/up bits");
        observation.events.Clear();
        observation.position = CVector(Float(0x7f800000), 0, 0);
        repair.redirectPosition = true;
        Check(ped.GetPosition() == &observation.alternate && observation.events.Text() == "PRP" && SameVector(observation.alternate, CVector(40, 50, 60)),
              "position repair rereads the actual alias after a matrix/position source replacement");
        Check(SameVector(repair.requested.vPos, CVector(40, 50, 60)), "matrix getter remembered its copied position before subsequent position repair");
        repair.redirectPosition = false;

        observation.events.Clear();
        const auto oldMatrixReads = observation.matrixReads, oldRepairCalls = repair.calls;
        Check(entity.GetMatrix(nullptr) == nullptr && observation.matrixReads == oldMatrixReads && repair.calls == oldRepairCalls,
              "null matrix output returns null without a fabricated observation or repair");
        observation.hasMatrix = false;
        const CMatrix sentinel(CVector(71, 72, 73));
        matrix = sentinel;
        Check(entity.GetMatrix(&matrix) == nullptr && SameMatrix(matrix, sentinel) && repair.calls == oldRepairCalls,
              "initially absent native matrix returns null and preserves caller storage");
        observation.hasMatrix = true;
        observation.nullPosition = true;
        Refuses([&] { entity.GetPosition(); }, "Ok status with a null borrowed position is still an explicit read failure");
        observation.nullPosition = false;
        const auto readCount = observation.velocityReads;
        Refuses([&] { physical.GetMoveSpeed(nullptr); }, "null linear output refuses before the typed owner");
        Refuses([&] { physical.GetTurnSpeed(nullptr); }, "null angular output refuses before the typed owner");
        Check(observation.velocityReads == readCount, "null SDK velocity outputs do not enter any native observation");

        for (std::uint32_t status = static_cast<std::uint32_t>(ClockResult::InvalidArgument);
             status <= static_cast<std::uint32_t>(ClockResult::OriginalException); ++status)
        {
            const auto result = static_cast<ClockResult>(status);
            observation.positionResult = result;
            const auto positionCalls = observation.positionReads, repairs = repair.calls;
            Refuses([&] { entity.GetPosition(); }, "every position observation refusal propagates without a native alias fallback");
            Check(observation.positionReads == positionCalls + 1 && repair.calls == repairs, "refused position is read once and never invokes repair");
            observation.positionResult = ClockResult::Ok;
            observation.matrixResult = result;
            matrix = sentinel;
            const auto matrixCalls = observation.matrixReads;
            Refuses([&] { entity.GetMatrix(&matrix); }, "every matrix observation refusal propagates explicitly");
            Check(observation.matrixReads == matrixCalls + 1 && SameMatrix(matrix, sentinel), "unacknowledged copied rows never reach caller matrix storage");
            observation.matrixResult = ClockResult::Ok;
            observation.velocityResult = result;
            output = CVector(7, 8, 9);
            const auto velocityCalls = observation.velocityReads;
            Refuses([&] { physical.GetMoveSpeed(&output); }, "every linear observation refusal propagates explicitly");
            Refuses([&] { physical.GetTurnSpeed(&output); }, "every angular observation refusal propagates explicitly");
            Check(observation.velocityReads == velocityCalls + 2 && SameVector(output, CVector(7, 8, 9)) && repair.calls == repairs,
                  "refused velocities preserve caller output without fallback, retry or repair");
            observation.velocityResult = ClockResult::Ok;
            observation.linear = CVector(16001, 0, 0);
            observation.events.Clear();
            repair.result = result;
            const auto acceptedRepairs = repair.calls;
            Refuses([&] { physical.GetMoveSpeed(&output); }, "every repair refusal propagates instead of reporting a repaired velocity");
            Check(repair.calls == acceptedRepairs + 1 && observation.events.Text() == "LR" && SameVector(observation.linear, CVector()),
                  "accepted owned repair effects survive failed acknowledgement with no second read, retry or rollback");
            Check(SameVector(output, CVector(16001, 0, 0)), "first observed invalid velocity remains caller-visible when later repair fails");
            repair.result = ClockResult::Ok;
        }

        observation.matrix = CMatrix(CVector(80, 81, 82));
        observation.matrix.vFront = CVector(Float(0x7fc01234), 0, 0);
        const auto invalidMatrix = observation.matrix;
        repair.result = ClockResult::WriteFailure;
        observation.events.Clear();
        Refuses([&] { entity.GetMatrix(&matrix); }, "matrix repair failure does not become a successful copied matrix");
        Check(observation.events.Text() == "MR" && SameMatrix(matrix, invalidMatrix),
              "first matrix output survives later repair failure at the original classic commit point");
        repair.result = ClockResult::Ok;
        observation.matrix.vFront = CVector(Float(0x7fc01234), 0, 0);
        repair.dropMatrix = true;
        observation.events.Clear();
        Refuses([&] { entity.GetMatrix(&matrix); }, "matrix disappearance during repair is an explicit binding refusal");
        Check(observation.events.Text() == "MRM", "lost second matrix is observed once without a fabricated identity or null dereference");
        repair.dropMatrix = false;
        observation.hasMatrix = true;
        // The failure paths must not remember an unacknowledged matrix position.
        observation.linear = CVector(16001, 0, 0);
        physical.GetMoveSpeed(&output);
        Check(SameVector(repair.requested.vPos, CVector(40, 50, 60)), "failed matrix/repair operations do not publish a new last-good position");

        observation.events.Clear();
        repair.apply = false;
        observation.angular = CVector(16001, 0, 0);
        physical.GetTurnSpeed(&output);
        Check(observation.events.Text() == "ARA" && SameVector(output, CVector(16001, 0, 0)),
              "a second invalid observation is returned after one repair without an invented native recovery loop");
        repair.apply = true;
        const auto totalObservations = observation.positionReads + observation.matrixReads + observation.velocityReads;
        {
            fixture::PedPhysicsFixture unused(vitals, metadata, mutation, observation, expectedToken, &PhysicsRepair::Invoke, &repair);
        }
        Check(observation.positionReads + observation.matrixReads + observation.velocityReads == totalObservations,
              "SDK destruction never reads native state or owns native reference retirement");
        fixture::PedPhysicsFixture unavailable(vitals, metadata, mutation, expectedToken);
        Refuses([&] { unavailable.GetPosition(); }, "default observation owner refuses a position alias");
        Refuses([&] { unavailable.GetMatrix(&matrix); }, "default observation owner refuses a copied matrix");
        Refuses([&] { unavailable.GetMoveSpeed(&output); }, "default observation owner refuses linear velocity");
        Refuses([&] { unavailable.GetTurnSpeed(&output); }, "default observation owner refuses angular velocity");
        std::puts("Production abstract ped physics SDK policy fixture completed");
    }
}
