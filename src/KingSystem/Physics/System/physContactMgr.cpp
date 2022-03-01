#include "KingSystem/Physics/System/physContactMgr.h"
#include <prim/seadScopedLock.h>
#include "KingSystem/Physics/System/physContactPointInfo.h"
#include "KingSystem/Physics/System/physContactPointInfoEx.h"
#include "KingSystem/Physics/System/physEntityGroupFilter.h"
#include "KingSystem/Physics/System/physGroupFilter.h"
#include "KingSystem/Physics/System/physSystem.h"

namespace ksys::phys {

ContactMgr::ContactMgr() {
    mRigidContactPoints.initOffset(ContactPointInfo::getListNodeOffset());
    // FIXME: figure out what these offsets are
    mList2.initOffset(0x78);
    mList3.initOffset(0x40);
    mList0.initOffset(0x10);
    mList4.initOffset(0x48);
    mList5.initOffset(0x48);
}

void ContactMgr::initContactPointPool(sead::Heap* heap, IsIndoorStage indoor) {
    int count = 0x1000;
    if (indoor == IsIndoorStage::Yes)
        count = 0x2000;
    mContactPointPool.allocBufferAssert(count, heap);
}

void ContactMgr::freeContactPointPool() {
    mContactPointPool.freeBuffer();
}

void ContactMgr::loadContactInfoTable(sead::Heap* heap, agl::utl::ResParameterArchive archive,
                                      ContactLayerType type) {
    auto& table = mContactInfoTables[int(type)];

    table.param_io.addList(&table.contact_info_table_plist, "ContactInfoTable");

    const auto root = archive.getRootList();
    const auto num_receivers = root.getResParameterObj(0).getNum();

    if (num_receivers != 0)
        table.receivers.allocBufferAssert(num_receivers, heap);

    doLoadContactInfoTable(archive, type, false);
}

void ContactMgr::doLoadContactInfoTable(agl::utl::ResParameterArchive archive,
                                        ContactLayerType type, bool skip_params) {
    auto& table = mContactInfoTables[int(type)];

    table.contact_info_table_plist.clearObj();

    const auto root = archive.getRootList();
    const auto names = root.getResParameterObj(0);

    const auto* filter = System::instance()->getGroupFilter(type);
    const auto layer_base = static_cast<int>(getContactLayerBase(type));

    for (int i = 0; i < table.receivers.size(); ++i) {
        auto& receiver = table.receivers[i];

        auto* receiver_name = names.getParameterData<const char>(i);
        receiver.name = receiver_name;

        table.contact_info_table_plist.addObj(&receiver, receiver_name);
        receiver.num_layers = filter->getNumLayers();

        if (!skip_params) {
            for (int idx = 0; idx < filter->getNumLayers(); ++idx) {
                const char* layer_name = contactLayerToText(layer_base + idx);
                receiver.layer_values[idx].init(0, layer_name, layer_name, &receiver);
            }
        }
    }

    table.param_io.applyResParameterArchive(archive);
}

ContactPointInfo* ContactMgr::allocContactPoints(sead::Heap* heap, int num,
                                                 const sead::SafeString& name, int a, int b,
                                                 int c) {
    auto* points = new (heap) ContactPointInfo(name, a, b, c);
    points->allocPoints(heap, num);
    return points;
}

ContactPointInfoEx* ContactMgr::allocContactPointsEx(sead::Heap* heap, int num, int num2,
                                                     const sead::SafeString& name, int a, int b,
                                                     int c) {
    auto* points = new (heap) ContactPointInfoEx(name, a, b, c);
    points->allocPoints(heap, num, num2);
    registerContactPointInfo(points);
    return points;
}

void ContactMgr::registerContactPointInfo(ContactPointInfoBase* info) {
    auto lock = sead::makeScopedLock(mMutex1);
    if (!info->isLinked())
        mRigidContactPoints.pushBack(info);
}

void ContactMgr::freeContactPointInfo(ContactPointInfoBase* info) {
    if (!info)
        return;

    {
        auto lock = sead::makeScopedLock(mMutex1);
        if (info->isLinked())
            mRigidContactPoints.erase(info);
    }
    info->freePoints();
    delete info;
}

bool ContactMgr::getSensorLayerMask(SensorCollisionMask* mask,
                                    const sead::SafeString& receiver_type) const {
    const auto& receivers = mContactInfoTables[int(ContactLayerType::Sensor)].receivers;
    for (int i = 0; i < receivers.size(); ++i) {
        const auto& receiver = receivers[i];
        if (receiver_type == receiver.name) {
            receiverMaskSetSensorLayerMask(mask, receiver.layer_mask);
            return true;
        }
    }
    return false;
}

void ContactInfoTable::Receiver::postRead_() {
    layer_mask = 0;
    layer_mask2 = 0;

    for (int i = 0; i < num_layers; ++i) {
        const auto value = layer_values[i].ref();
        if (value & 1)
            layer_mask |= 1 << i;
        if (value & 2)
            layer_mask2 |= 1 << i;
    }
}

}  // namespace ksys::phys
