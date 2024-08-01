#pragma once
#include "defs.h"

#include "rpc_deferrable.h"

#include <ydb/public/api/protos/ydb_bsconfig.pb.h>
#include <ydb/core/blobstorage/base/blobstorage_events.h>
#include <ydb/core/base/tabletid.h>
#include <ydb/library/ydb_issue/issue_helpers.h>
#include <ydb/core/cms/console/configs_dispatcher.h>
#include <ydb/core/ydb_convert/ydb_convert.h>
#include <ydb/public/lib/operation_id/operation_id.h>
#include <ydb/public/sdk/cpp/client/resources/ydb_resources.h>

namespace NKikimr {
namespace NGRpcService {

struct TDriveDevice {
    TString path;
    NKikimrBlobStorage::EPDiskType type;

    bool operator<(const TDriveDevice& other) const {
        return std::tie(path, type) < std::tie(other.path, other.type);
    }

    auto operator<=>(const TDriveDevice &) const = default;
};

template <typename TResult>
Ydb::StatusIds::StatusCode PullStatus(const TResult& /*status*/) {
    return Ydb::StatusIds::SUCCESS;
}

}
}

template <>
struct THash<NKikimr::NGRpcService::TDriveDevice> {
    std::size_t operator()(const NKikimr::NGRpcService::TDriveDevice &device) const {
        return THash<TString>()(device.path) ^ THash<NKikimrBlobStorage::EPDiskType>()(device.type);
    }
};

namespace NKikimr{
namespace NGRpcService{

class TDriveDeviceSet {
public:
    void AddDevice(const TDriveDevice& device) {
        if (Devices.insert(device).second) {
            Hash ^= THash<TDriveDevice>()(device);
        }
    }

    void RemoveDevice(const TDriveDevice& device) {
        if (Devices.erase(device)) {
            Hash ^= THash<TDriveDevice>()(device);
        }
    }

    std::size_t GetHash() const {
        return Hash;
    }

    const THashSet<TDriveDevice>& GetDevices() const {
        return Devices;
    }

    bool operator==(const TDriveDeviceSet& other) const {
        return Devices == other.Devices;
    }

private:    
    THashSet<TDriveDevice> Devices;
    std::size_t Hash = 0;
};


void CopyToConfigRequest(const Ydb::BSConfig::InitRequest &from, NKikimrBlobStorage::TConfigRequest *to);

template <typename TDerived>
class TBaseBSConfigRequest {
protected:
    void OnBootstrap() {
        auto self = static_cast<TDerived*>(this);
        Ydb::StatusIds::StatusCode status = Ydb::StatusIds::STATUS_CODE_UNSPECIFIED;
        NYql::TIssues issues;
        if (!self->ValidateRequest(status, issues)) {
            self->Reply(status, issues, self->ActorContext());
            return;
        }
        if (const auto& userToken = self->Request_->GetSerializedToken()) {
            UserToken = new NACLib::TUserToken(userToken);
        }
    }

    bool CheckAccess(const TString& path, TIntrusivePtr<TSecurityObject> securityObject, ui32 access) {
        auto self = static_cast<TDerived*>(this);
        if (!UserToken || !securityObject) {
            return true;
        }

        if (securityObject->CheckAccess(access, *UserToken)) {
            return true;
        }

        self->Reply(Ydb::StatusIds::UNAUTHORIZED,
            TStringBuilder() << "Access denied"
                << ": for# " << UserToken->GetUserSID()
                << ", path# " << path
                << ", access# " << NACLib::AccessRightsToString(access),
            NKikimrIssues::TIssuesIds::ACCESS_DENIED,
            self->ActorContext());
        return false;
    }

private:
    TIntrusiveConstPtr<NACLib::TUserToken> UserToken;
};

template <typename TDerived, typename TRequest, typename TResultRecord>
class TBSConfigRequestGrpc : public TRpcOperationRequestActor<TDerived, TRequest>
                           , public TBaseBSConfigRequest<TBSConfigRequestGrpc<TDerived, TRequest, TResultRecord>> {
    using TBase = TRpcOperationRequestActor<TDerived, TRequest>;

    template<typename T, typename = void>
    struct THasMsg: std::false_type
    {};
    template<typename T>
    struct THasMsg<T, std::enable_if_t<std::is_same<decltype(std::declval<T>().msg()), void>::value>>: std::true_type
    {};
    template<typename T>
    static constexpr bool HasMsgV = THasMsg<T>::value;

    friend class TBaseBSConfigRequest<TBSConfigRequestGrpc<TDerived, TRequest, TResultRecord>>;
public:
    TBSConfigRequestGrpc(IRequestOpCtx* request)
        : TBase(request) {}

    void Bootstrap(const TActorContext &ctx) {
        TBase::Bootstrap(ctx);
        this->OnBootstrap();
        this->Become(&TBSConfigRequestGrpc::StateFunc);
        BSCTabletId = MakeBSControllerID();
        CreatePipe();
        SendRequest();
    }

protected:
    STFUNC(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvTabletPipe::TEvClientConnected, Handle);
            hFunc(TEvTabletPipe::TEvClientDestroyed, Handle);
            hFunc(TEvBlobStorage::TEvControllerConfigResponse, Handle);
        default:
            return TBase::StateFuncBase(ev);
        }
    }

    NTabletPipe::TClientConfig GetPipeConfig() {
        NTabletPipe::TClientConfig cfg;
        cfg.RetryPolicy = {
            .RetryLimitCount = 3u
        };
        return cfg;
    }

    void CreatePipe() {
        BSCPipeClient = this->Register(NTabletPipe::CreateClient(this->SelfId(), BSCTabletId, GetPipeConfig()));
    }

    void SendRequest() {
        std::unique_ptr<TEvBlobStorage::TEvControllerConfigRequest> req = std::make_unique<TEvBlobStorage::TEvControllerConfigRequest>();
        auto &rec = *this->GetProtoRequest();
        CopyToConfigRequest(rec, req->Record.MutableRequest());
        NTabletPipe::SendData(this->SelfId(), BSCPipeClient, req.release(), 0, TBase::Span_.GetTraceId());
    }

    void Handle(typename TEvBlobStorage::TEvControllerConfigResponse::TPtr &ev) {
        auto status = PullStatus(ev->Get()->Record);
        if constexpr (HasMsgV<decltype(ev->Get()->Record)>) {
            if (status != Ydb::StatusIds::SUCCESS) {
                this->Reply(status, ev->Get()->Record.GetResponse().GetErrorDescription(), NKikimrIssues::TIssuesIds::DEFAULT_ERROR, this->ActorContext());
            }
        }
        TResultRecord result;
        //CopyProtobuf(ev->Get()->Record, &result);
        this->ReplyWithResult(status, result, TActivationContext::AsActorContext());
    }

    void Handle(TEvTabletPipe::TEvClientConnected::TPtr& ev) {
        if (ev->Get()->Status != NKikimrProto::OK) {
            this->Reply(Ydb::StatusIds::UNAVAILABLE, "Failed to connect to coordination node.", NKikimrIssues::TIssuesIds::SHARD_NOT_AVAILABLE, this->ActorContext());
        }
    }

    void Handle(TEvTabletPipe::TEvClientDestroyed::TPtr&) {
        this->Reply(Ydb::StatusIds::UNAVAILABLE, "Connection to coordination node was lost.", NKikimrIssues::TIssuesIds::SHARD_NOT_AVAILABLE, this->ActorContext());
    }

    virtual bool ValidateRequest(Ydb::StatusIds::StatusCode& status, NYql::TIssues& issues) = 0;
private:
    ui64 BSCTabletId = 0;
    TActorId BSCPipeClient;
};

} // namespace NGRpcService
} // namespace NKikimr

template <>
struct THash<NKikimr::NGRpcService::TDriveDeviceSet> {
    std::size_t operator()(const NKikimr::NGRpcService::TDriveDeviceSet &deviceSet) const {
        return deviceSet.GetHash();
    }
};
