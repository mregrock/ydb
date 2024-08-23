#include "service_keyvalue.h"
#include "rpc_bsconfig_base.h"

#include <ydb/core/base/path.h>
#include <ydb/core/grpc_services/rpc_common/rpc_common.h>
#include <ydb/core/mind/local.h>
#include <ydb/core/protos/local.pb.h>

namespace NKikimr::NGRpcService {

using TEvDefineStorageConfigRequest =
    TGrpcRequestOperationCall<Ydb::BSConfig::DefineStorageConfigRequest,
        Ydb::BSConfig::DefineStorageConfigResponse>;
using TEvFetchStorageConfigRequest =
    TGrpcRequestOperationCall<Ydb::BSConfig::FetchStorageConfigRequest,
        Ydb::BSConfig::FetchStorageConfigResponse>;

using namespace NActors;
using namespace Ydb;

bool CopyToConfigRequest(const Ydb::BSConfig::DefineStorageConfigRequest &from, NKikimrBlobStorage::TConfigRequest *to) {
    auto& storageConfig = from.storage_config();
    for (const auto& hostConfig: storageConfig.host_config()) {
        auto *defineHostConfig = to->AddCommand()->MutableDefineHostConfig();
        defineHostConfig.SetHostConfigId(hostConfig.host_config_id());
        for (const auto& drive: hostConfig.drive()) {
            auto *newDrive = defineHostConfig.AddDrive();
            newDrive.SetPath(drive.path());
            newDrive.SetType(static_cast<decltype(newDrive.GetType())>(drive.type()));
            newDrive.SetSharedWithOs(drive.shared_with_os());
            newDrive.SetReadCentric(drive.read_centric());
            newDrive.SetKind(drive.kind());
            newDrive.MutablePDiskConfig()->SetExpectedSlotCount(drive.expected_slot_count());
        }
        defineHostConfig.SetItemConfigGeneration(storageConfig.item_config_generation());
    }
    auto *defineBox = to->AddCommand()->MutableDefineBox();
    defineBox->SetBoxId(1);
    defineBox->SetItemConfigGeneration(from.item_config_generation());
    for (const auto& host: storageConfig.host()) {
        auto *newHost = defineBox.AddHost();
        newHost->SetHostConfigId(host.host_config_id());
        auto *newHostKey = newHost->MutableHostKey();
        auto& hostKey = host.host_key();
        if (hostKey.has_node_id()) {
            newHostKey->SetNodeId(hostKey.node_id());
        }
        else {
            newHostKey->SetFqdn(hostKey.endpoint().fqdn());
            newHostKey->SetIcPort(hostKey.endpoint().ic_port());
        }
    }
    return true;
}

void CopyFromConfigResponse(const NKikimrBlobStorage::TConfigResponse &/*from*/, Ydb::BSConfig::DefineStorageConfigResult */*to*/) {
}

bool CopyToConfigRequest(const Ydb::BSConfig::FetchStorageConfigRequest &from, NKikimrBlobStorage::TConfigRequest *to) {
    to->AddCommand()->MutableReadHostConfig();
    to->AddCommand()->MutableReadBox();
    return true;
}

void CopyFromConfigResponse(const NKikimrBlobStorage::TConfigResponse &from, Ydb::BSConfig::FetchStorageConfigResult *to) {
    auto HostConfigStatus = from.GetStatus()[0];
    auto Box = from.GetStatus()[1];
    auto *storageConfig = to->mutable_storage_config();
    for (const auto& hostConfig: HostConfigStatus.GetHostConfig()) {
        if (command.has_define_host_config()) {
            auto& defineHostConfig = command.define_host_config();
            auto *hostConfig = storageConfig.add_host_config();
            hostConfig->set_host_config_id(defineHostConfig.host_config_id());
            for (const auto& drive : defineHostConfig.drive()) {
                auto *newDrive = hostConfig->add_drive();
                newDrive->set_path(drive.path());
                newDrive->set_type(drive.type());
                newDrive->set_shared_with_os(drive.shared_with_os());
                newDrive->set_read_centric(drive.read_centric());
                newDrive->set_kind(drive.kind());
                newDrive->mutable_pdisk_config()->set_expected_slot_count(drive.pdisk_config().expected_slot_count());
            }
            storageConfig.set_item_config_generation(defineHostConfig.item_config_generation());
        }
        else if (command.has_define_box()) {
            auto& defineBox = command.define_box();
            to->set_item_config_generation(defineBox.item_config_generation());
            for (const auto& host : defineBox.host()) {
                auto *newHost = storageConfig.add_host();
                newHost->set_host_config_id(host.host_config_id());
                auto *newHostKey = newHost->mutable_host_key();
                const auto& hostKey = host.host_key();
                if (hostKey.has_node_id()) {
                    newHostKey->set_node_id(hostKey.node_id());
                }
                else {
                    auto *endpoint = newHostKey->mutable_endpoint();
                    endpoint->set_fqdn(hostKey.endpoint().fqdn());
                    endpoint->set_ic_port(hostKey.endpoint().ic_port());
                }
            }
        }
    }

}

class TDefineStorageConfigRequest : public TBSConfigRequestGrpc<TDefineStorageConfigRequest, TEvDefineStorageConfigRequest,
    Ydb::BSConfig::DefineStorageConfigResult> {
public:
    using TBase = TBSConfigRequestGrpc<TDefineStorageConfigRequest, TEvDefineStorageConfigRequest, Ydb::BSConfig::DefineStorageConfigResult>;
    using TBase::TBase;

    bool ValidateRequest(Ydb::StatusIds::StatusCode& /*status*/, NYql::TIssues& /*issues*/) override {
        return true;
    }
    NACLib::EAccessRights GetRequiredAccessRights() const {
        return NACLib::GenericManage;
    }
};

class TFetchStorageConfigRequest : public TBSConfigRequestGrpc<TFetchStorageConfigRequest, TEvFetchStorageConfigRequest,
    Ydb::BSConfig::FetchStorageConfigResult> {
public:
    using TBase = TBSConfigRequestGrpc<TFetchStorageConfigRequest, TEvFetchStorageConfigRequest, Ydb::BSConfig::FetchStorageConfigResult>;
    using TBase::TBase;

    bool ValidateRequest(Ydb::StatusIds::StatusCode& /*status*/, NYql::TIssues& /*issues*/) override {
        return true;
    }
    NACLib::EAccessRights GetRequiredAccessRights() const {
        return NACLib::GenericManage;
    }
};

void DoDefineStorageConfigBSConfig(std::unique_ptr<IRequestOpCtx> p, const IFacilityProvider&) {
    TActivationContext::AsActorContext().Register(new TDefineStorageConfigRequest(p.release()));
}

void DoFetchStorageConfigBSConfig(std::unique_ptr<IRequestOpCtx> p, const IFacilityProvider&) {
    TActivationContext::AsActorContext().Register(new TFetchStorageConfigRequest(p.release()));
}


} // namespace NKikimr::NGRpcService
