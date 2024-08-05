#include "service_keyvalue.h"
#include "rpc_bsconfig_base.h"

#include <ydb/core/base/path.h>
#include <ydb/core/grpc_services/rpc_common/rpc_common.h>
#include <ydb/core/mind/local.h>
#include <ydb/core/protos/local.pb.h>

namespace NKikimr::NGRpcService {

using TEvInitRequest =
    TGrpcRequestOperationCall<Ydb::BSConfig::InitRequest,
        Ydb::BSConfig::InitResponse>;

using namespace NActors;
using namespace Ydb;

void CopyToConfigRequest(const Ydb::BSConfig::InitRequest &from, NKikimrBlobStorage::TConfigRequest *to) {
    THashMap<TDriveDeviceSet, ui64> hostConfigMap;
    ui64 hostConfigId = 0;
    auto *defineBox = to->AddCommand()->MutableDefineBox();
    defineBox->SetBoxId(1);

    for (const auto& driveInfo: from.drive_info()) {
        TDriveDeviceSet driveSet;

        for (const auto& drive: driveInfo.drive()) {
            TDriveDevice device{drive.path(), static_cast<NKikimrBlobStorage::EPDiskType>(drive.type())};
            driveSet.AddDevice(device);
        }

        if (hostConfigMap.find(driveSet) == hostConfigMap.end()) {
            auto *hostConfig = to->AddCommand()->MutableDefineHostConfig();
            hostConfig->SetHostConfigId(++hostConfigId);

            for (const auto& device: driveSet.GetDevices()) {
                auto *hostConfigDrive = hostConfig->AddDrive();
                hostConfigDrive->SetPath(device.path);
                hostConfigDrive->SetType(static_cast<decltype(hostConfigDrive->GetType())>(device.type));
            }

            hostConfigMap[driveSet] = hostConfigId;
        }

        auto *host = defineBox->AddHost();
        host->MutableKey()->SetFqdn(driveInfo.fqdn());
        host->MutableKey()->SetIcPort(driveInfo.port());
        host->SetEnforcedNodeId(driveInfo.node_id());
        host->SetHostConfigId(hostConfigMap[driveSet]);
    }
}

void CopyFromConfigResponse(const NKikimrBlobStorage::TConfigResponse &/*from*/, Ydb::BSConfig::InitResult */*to*/) {
}

class TInitRequest : public TBSConfigRequestGrpc<TInitRequest, TEvInitRequest, Ydb::BSConfig::InitResult> {
public:
    using TBase = TBSConfigRequestGrpc<TInitRequest, TEvInitRequest, Ydb::BSConfig::InitResult>;
    using TBase::TBase;

    bool ValidateRequest(Ydb::StatusIds::StatusCode& /*status*/, NYql::TIssues& /*issues*/) override {
        return true;
    }
    NACLib::EAccessRights GetRequiredAccessRights() const {
        return NACLib::UpdateRow;
    }
};

void DoInitRequest(std::unique_ptr<IRequestOpCtx> p, const IFacilityProvider&) {
    TActivationContext::AsActorContext().Register(new TInitRequest(p.release()));
}


} // namespace NKikimr::NGRpcService
