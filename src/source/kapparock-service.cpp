//====================================================================================
//			The MIT License (MIT)
//
//			Copyright (c) 2011 Kapparock LLC
//
//			Permission is hereby granted, free of charge, to any person obtaining a copy
//			of this software and associated documentation files (the "Software"), to deal
//			in the Software without restriction, including without limitation the rights
//			to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//			copies of the Software, and to permit persons to whom the Software is
//			furnished to do so, subject to the following conditions:
//
//			The above copyright notice and this permission notice shall be included in
//			all copies or substantial portions of the Software.
//
//			THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//			IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//			FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//			AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//			LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//			OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//			THE SOFTWARE.
//====================================================================================

#include "notification.h"
#include "kjson.h"
#include "restful.h"
#include "kutil.h"
#include "hal.h"
#include "kapparock-service.h"
#include "zdo.h"
#include "zdo_archive.h"

using namespace kapi;

void init()
{
	using Context = ApplicationInterface::Context;

	notify::handler("Application", "rsserial/restart", [](Context ctx)
	{
		JSON rsp(JSONType::JSON_OBJECT);
		JSON arg(ctx.parameter().c_str());
		JSON req(ctx.request().c_str());
		rsp["status"] = -1;
		rsp["message"] = "http verb is not supported";

		if (req["method"].toString() == "POST") {
			int t=0;
			if (arg.exist("delay")) {
				t = arg["delay"].toInteger();
			}
			rsp["status"] = 0;
			rsp["message"] = "rsserial is going down";
			HAL::delayRestart(t);
		}
		ctx.response(rsp.stringify());
	});

	notify::handler("Application", "rsserial/startup_state", [](Context ctx)
	{
		JSON rsp(JSONType::JSON_OBJECT);
		JSON arg(ctx.parameter().c_str());
		JSON req(ctx.request().c_str());
		rsp["status"] = -1;
		rsp["message"] = "http verb is not supported";

		if (req["method"].toString() == "POST") {
			int restart = 0;
			int default_= 0;
			rsp["status"] = 0;
			rsp["message"] = "Invalid parameter";
			if (arg.exist("restart")) {
				restart = arg["restart"].toInteger();
			}

			if (arg.exist("default")) {
				default_ = arg["default"].toInteger();
			}

			if (default_ == 1)
			{
				HAL::zcdStartupOptions(0x03);
				ZDO::ARCHIVE::clear();

				if (restart == 1)
				{
					rsp["message"] = "Restored to default network state, going down to restart...";
					HAL::delayRestart(2);
				} else {
					rsp["message"] = "Restored to default network state, restart is need";
				}
			}
		}
		ctx.response(rsp.stringify());
	});

	notify::handler("Application", "zigbee_module/assoc_count", [](Context ctx)
	{
		JSON rsp(JSONType::JSON_OBJECT);
		int count = getAssocCount();
		rsp["assoc_count"] = count;
		ctx.response(rsp.stringify());
	});

	notify::handler("Application", "zigbee_module/assoc_find_device", [](Context ctx)
	{
		JSON arg(ctx.parameter().c_str());
		HAL_STRUCTURE::associated_devices_t dev;
		JSON rsp(JSONType::JSON_OBJECT);
		uint8_t number = arg["number"].toInteger();

		int status = getAssocInfo(&dev, number);
		rsp["status"] = status;
		rsp["device"] = { kapi::JSONType::JSON_OBJECT };
		JSON& devJson = rsp["device"];
		devJson["nwkAddr"] =  IntToHexStr(dev.shortAddr);
		devJson["ieeeAddr"] = IntToHexStr(getAssocExtAddr(dev.shortAddr));
		devJson["age"] =  dev.age;
		devJson["assocCnt"] = dev.assocCnt;
		devJson["devStatus"] = IntToHexStr(dev.devStatus);
		devJson["nodeRelation"] = dev.nodeRelation;
		ctx.response(rsp.stringify());
	});

	notify::handler("Application", "zigbee_module/get_nv_info", [](Context ctx)
	{
		JSON arg(ctx.parameter().c_str());
		HAL::NvInfo_t nvInfo;
		JSON rsp(JSONType::JSON_OBJECT);

		int status = HAL::getNvInfo(nvInfo);
		rsp["status"] = status;
		rsp["nv_info"] = { kapi::JSONType::JSON_OBJECT };
		JSON& devJson = rsp["nv_info"];
		devJson["status"] = nvInfo.status;
		devJson["IEEEAddr"] = IntToHexStr(nvInfo.IEEEAddr);
		devJson["ScanChannels"] = IntToHexStr(nvInfo.ScanChannels);
		devJson["PanId"] = IntToHexStr(nvInfo.PanId);
		devJson["SecurityLevel"] = IntToHexStr(nvInfo.SecurityLevel);

		ctx.response(rsp.stringify());
	});

	notify::handler("Application", "zigbee_module/nv_item", [](Context ctx)
	{
		JSON arg(ctx.parameter().c_str());
		JSON req(ctx.request().c_str());
		JSON rsp(JSONType::JSON_OBJECT);
		rsp["status"] = -1;

		// "id" and "offset" are required for both read and write
		if (!arg.exist("id")) {
			rsp["status"] = -1;
			rsp["message"] = "id is missing";
			ctx.response(rsp.stringify());
			return;
		}

		if (!arg.exist("offset")) {
			rsp["status"] = -1;
			rsp["message"] = "offset is missing";
			ctx.response(rsp.stringify());
			return;
		}

		if (req["method"].toString() == "GET") {

			HAL::NvItem_t nvItem;
			nvItem.id = static_cast<uint16_t>(arg["id"].toInteger());;
			nvItem.offset = static_cast<uint8_t>(arg["offset"].toInteger());

			int status = HAL::OsalNvRead(nvItem);

			rsp["status"] = status;
			rsp["nv_item"] = { kapi::JSONType::JSON_OBJECT };
			if (status == 0) {
				JSON& devJson = rsp["nv_item"];
				devJson["status"] = nvItem.status;
				devJson["id"] = IntToHexStr(nvItem.id);
				devJson["offset"] = IntToHexStr(nvItem.offset);
				devJson["len"] = nvItem.len;
				char temp[512];
				int len = kByteToHexString(temp,nvItem.value,nvItem.len);
				std::string str(temp,len);
				devJson["value"] = str;
			}
			ctx.response(rsp.stringify());
			return;

		} else if (req["method"].toString() == "POST") {

			HAL::NvItem_t nvItem;
			if (!arg.exist("len")) {
				rsp["status"] = -1;
				rsp["message"] = "len is missing";
				ctx.response(rsp.stringify());
				return;
			}
			if (!arg.exist("value")) {
				rsp["status"] = -1;
				rsp["message"] = "value is missing";
				ctx.response(rsp.stringify());
				return;
			}

			nvItem.id = static_cast<uint16_t>(arg["id"].toInteger());;
			nvItem.offset = static_cast<uint8_t>(arg["offset"].toInteger());
			nvItem.len = static_cast<uint8_t> (arg["len"].toInteger());
			kHexStringToByte(nvItem.value, arg["value"].toString().c_str(), (static_cast<int>(nvItem.len)) * 2);

			rsp["status"] = 0;
			rsp["message"] = "Executed NV write";
			rsp["nv_item"] = { kapi::JSONType::JSON_OBJECT };

			int status = HAL::OsalNvWrite(nvItem);

			if (status == 0)
			{
				JSON& devJson = rsp["nv_item"];
				devJson["status"] = nvItem.status;
				devJson["len"] = IntToHexStr(nvItem.len);
				char temp[512];
				int len = kByteToHexString(temp,nvItem.value,nvItem.len);
				std::string str(temp,len);
				devJson["value"] = str;
			}

			ctx.response(rsp.stringify());

			return;

		} else {
			rsp["status"] = -1;
			rsp["message"] = "http verb is not supported";
			ctx.response(rsp.stringify());
		}
	});

	notify::handler("Application", "zigbee_module/logical_channel", [](Context ctx)
	{
		JSON req(ctx.request().c_str());
		JSON rsp(JSONType::JSON_OBJECT);
		rsp["status"] = -1;

		if (req["method"].toString() == "GET") {
			uint8_t channel;
			int status = HAL::getChannel(channel);
			rsp["status"] = status;
			rsp["channel"] = { kapi::JSONType::JSON_OBJECT };

			if (status == 0)
			{
				JSON& devJson = rsp["channel"];
				devJson["number"] = IntToHexStr(channel);
			}
			ctx.response(rsp.stringify());

		} else if (req["method"].toString() == "POST") {
			JSON arg(ctx.parameter().c_str());
			if (!arg.exist("number"))
			{
				rsp["message"] ="Missing channel number";
				ctx.response(rsp.stringify());
			} else {
				uint8_t channel = static_cast<uint8_t> (arg["number"].toInteger());
				uint8_t currentChannel;
				uint8_t nwkUpdateId = 0;

				HAL::getNwkUpdateId(nwkUpdateId);
				HAL::getChannel(currentChannel);

				if (currentChannel != channel) {
					if (nwkUpdateId == 0xff) {
						nwkUpdateId = 0;
						HAL::setNwkUpdateId(nwkUpdateId);
					}
					nwkUpdateId++;
					ZDO::Mgmt_NWK_Update_req_frm frame{0x0001<<channel, 0xfe, 0xff, nwkUpdateId, 0x0000};
					ZDO::APDU{0x0000, frame.clusteId(), frame}.send([](AFMessage& x)
					{});
				}
				rsp["status"] = 0;
				rsp["message"] = "ok";
				rsp["nwkUpdateId"] = IntToHexStr(nwkUpdateId);
				rsp["currentChannel"] = IntToHexStr(currentChannel);
				rsp["channel"] = IntToHexStr(channel);
				ctx.response(rsp.stringify());
			}
		} else {
			rsp["message"] = "http verb is not supported";
			ctx.response(rsp.stringify());
		}
		//ctx.response(ctx.request());
	});
	return;
}
