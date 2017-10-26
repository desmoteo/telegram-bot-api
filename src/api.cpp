#include <tgbot/methods/api.h>
#include <tgbot/utils/https.h>
#include <tgbot/bot.h>
#include <json/json.h>
#include <sstream>

#define BOOL_TOSTR(xvalue) \
	((xvalue) ? "true" : "false")

#define SEPARATE(k, sstr) \
    if(k)\
        sstr << ','

using namespace tgbot::methods;
using namespace tgbot::utils;

static inline std::string toString(const types::ShippingOption& shippingOption) {
    std::stringstream jsonify;
    jsonify << "{ \"id\": \"" << shippingOption.id << "\", \"title\": \"" << shippingOption.title << "\""
            << ", \"prices\": [";

    const size_t& nPrices = shippingOption.prices.size();
    for(size_t i = 0; i < nPrices; i++) {
        SEPARATE(i,jsonify);

        const types::LabeledPrice& price = shippingOption.prices.at(i);
        jsonify << "{ \"amount\":" << price.amount << ", \"label\":\"" << price.label << "\" }";
    }

    jsonify << "]}";

    return jsonify.str();
}

static inline void allowedUpdatesToString(const std::vector<api_types::UpdateType>& updates,
                                    std::stringstream& stream) {
    stream << "[";
    for (auto const &update : updates) {
        switch (update) {
            case api_types::UpdateType::MESSAGE:
                stream << "\"message\",";
                break;
            case api_types::UpdateType::CALLBACK_QUERY:
                stream << "\"callback_query\",";
                break;
            case api_types::UpdateType::INLINE_QUERY :
                stream << "\"inline_query\",";
                break;
            case api_types::UpdateType::CHOSEN_INLINE_RESULT:
                stream << "\"chosen_inline_result\",";
                break;
            case api_types::UpdateType::PRE_CHECKOUT_QUERY:
                stream << "\"pre_checkout_query\",";
                break;
            case api_types::UpdateType::SHIPPING_QUERY:
                stream << "\"shipping_query\",";
                break;
            case api_types::UpdateType::EDITED_CHANNEL_POST:
                stream << "\"edited_channel_post\",";
                break;
            case api_types::UpdateType::EDITED_MESSAGE:
                stream << "\"edited_message\",";
                break;
            case api_types::UpdateType::CHANNEL_POST:
                stream << "\"channel_post\",";
                break;
        }
    }
}

static inline void removeComma(const std::stringstream& stream, std::string& target) {
    std::string &&req = stream.str();
    char &endpos = req.at(req.size() - 1);
    if (endpos == ',')
        endpos = ']';
    target = req;
}

static std::string toString(const api_types::MaskPosition& maskPosition) {
    std::stringstream jsonify;
    jsonify << "{ \"point\": \"" << maskPosition.point << "\",\"x_shift\": " << maskPosition.xShift
            << ",\"y_shift\": " << maskPosition.yShift << ",\"scale\": " << maskPosition.scale;

    return jsonify.str();
}

//Api constructors

//Webhook, no further action
tgbot::methods::Api::Api(const std::string& token) :
    baseApi("https://api.telegram.org/bot" + token)
{}

//Webhook
tgbot::methods::Api::Api(const std::string& token,
		const std::string& url,
		const int& maxConnections,
		const std::vector<api_types::UpdateType>& allowedUpdates) :
            baseApi("https://api.telegram.org/bot" + token) {

        if(!setWebhook(url,maxConnections,allowedUpdates))
            throw TelegramException("Unable to set webhook");
}

//Webhook with cert
tgbot::methods::Api::Api(const std::string& token,
		const std::string& url,
		const std::string& certificate,
		const int& maxConnections,
		const std::vector<api_types::UpdateType>& allowedUpdates) :
            baseApi("https://api.telegram.org/bot" + token) {

        if(!setWebhook(url,certificate,maxConnections,allowedUpdates))
            throw TelegramException("Unable to set webhook");
}

//HTTP Long polling
tgbot::methods::Api::Api(const std::string &token,
		const std::vector<api_types::UpdateType> &allowedUpdates,
		const int &timeout,
		const int &limit) :
	        baseApi("https://api.telegram.org/bot" + token),
	        currentOffset(0) {

		std::stringstream fullApiRequest;
		fullApiRequest << baseApi << "/getUpdates?limit=" << limit << "&timeout=" << timeout;

		if (allowedUpdates.size()) {
            fullApiRequest << "&allowed_updates=";
			allowedUpdatesToString(allowedUpdates,fullApiRequest);
			removeComma(fullApiRequest,updateApiRequest);
		} else 
			updateApiRequest = fullApiRequest.str();
}

//
//API implementations
//

//getUpdates (internal usage)
int tgbot::methods::Api::getUpdates(void *c, std::vector<api_types::Update>& updates) {
	std::stringstream updatesRequest;
	updatesRequest << updateApiRequest << "&offset=" << currentOffset;

	Json::Value rootUpdate;
	Json::Reader parser;

	parser.parse(utils::http::get(c, updatesRequest.str()), rootUpdate);

	if (!rootUpdate.get("ok", "").asBool())
		throw TelegramException(rootUpdate.get("description", "").asCString());

	Json::Value valueUpdates = rootUpdate.get("result", "");
	const int& updatesCount = valueUpdates.size();
	if (!updatesCount)
		return 0;

	for (auto const &singleUpdate : valueUpdates)
		updates.emplace_back(singleUpdate);

	currentOffset = 1 + valueUpdates[updatesCount - 1].get("update_id", "").asInt64();

	return updatesCount;
}

//
// Availible API Methods
//

//setWebhook
bool tgbot::methods::Api::setWebhook(const std::string &url, const int &maxConnections,
                                     const std::vector<api_types::UpdateType> &allowedUpdates) {
    std::stringstream request;
    request << baseApi << "/setWebhook?url=" << url << "&max_connections=" << maxConnections;

    std::string setWebhookRequest;
    if (allowedUpdates.size()) {
        request << "&allowed_updates=";
        allowedUpdatesToString(allowedUpdates,request);
        removeComma(request,setWebhookRequest);
    } else
        setWebhookRequest = request.str();

    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,setWebhookRequest),value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    urlWebhook = url;
    return true;
}

bool tgbot::methods::Api::setWebhook(const std::string &url, const std::string &certificate,
                                     const int &maxConnections,
                                     const std::vector<api_types::UpdateType> &allowedUpdates) {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    if(!allowedUpdates.size())
        reader.parse(http::multiPartUpload(inst, baseApi + "/setWebhook", certificate, url, maxConnections),
                     value);
    else {
        std::stringstream request;
        std::string final;

        allowedUpdatesToString(allowedUpdates, request);
        removeComma(request, final);

        reader.parse(http::multiPartUpload(inst, baseApi + "/setWebhook", certificate, url, maxConnections, final),
                     value);
    }

    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    urlWebhook = url;
    return true;
}

//deleteWebhook
bool tgbot::methods::Api::deleteWebhook() const {
    CURL* inst = http::curlEasyInit();
    bool isOk = (http::get(inst,baseApi + "/deleteWebhook").find("\"ok\":true") != std::string::npos);
    curl_easy_cleanup(inst);

    if(!isOk)
        throw TelegramException("Cannot delete webhook");

    return true;
}

//getWebhookInfo
api_types::WebhookInfo tgbot::methods::Api::getWebhookInfo() const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/getWebhookInfo"), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return api_types::WebhookInfo(value.get("result",""));
}

//getMe
api_types::User tgbot::methods::Api::getMe() const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/getMe"), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return api_types::User(value.get("result",""));
}

//getChat
api_types::Chat tgbot::methods::Api::getChat(const std::string &chatId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/getChat?chat_id=" + chatId), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return api_types::Chat(value.get("result",""));
}

//getChatMembersCount
unsigned tgbot::methods::Api::getChatMembersCount(const std::string &chatId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/getChatMembersCount?chat_id=" + chatId), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return value.get("result","").asUInt();
}

//getFile
api_types::File tgbot::methods::Api::getFile(const std::string &fileId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/getFile?file_id=" + fileId), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return api_types::File(value.get("result",""));
}

//getChatMember
api_types::ChatMember tgbot::methods::Api::getChatMember(const std::string &chatId, const int &userId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/getChatMember?chat_id=" << chatId << "&user_id=" << userId;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return api_types::ChatMember(value.get("result",""));
}

//getStickerSet
api_types::StickerSet tgbot::methods::Api::getStickerSet(const std::string &name) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/getStickerSet?name=" + name), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return api_types::StickerSet(value.get("result",""));
}

//getUserProfilePhotos
api_types::UserProfilePhotos tgbot::methods::Api::getUserProfilePhotos(const int &userId, const unsigned int &offset,
                                                                       const unsigned int &limit) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/getUserProfilePhotos?user_id=" << userId << "&offset=" << offset
            << "&limit=" << limit;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return api_types::UserProfilePhotos(value.get("result",""));
}

//getChatAdministrators
std::vector<api_types::ChatMember> tgbot::methods::Api::getChatAdministrators(const std::string &chatId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/getChatAdministrators?chat_id=" + chatId), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    std::vector<api_types::ChatMember> members;
    for(auto const member : value.get("result",""))
        members.emplace_back(member);

    return members;
}

//getGameHighScores
std::vector<api_types::GameHighScore> tgbot::methods::Api::getGameHighScores(const int &userId, const int &chatId,
                                                                             const int &messageId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/getGameHighScores?user_id=" << userId << "&chat_id=" << chatId
            << "&message_id=" << messageId;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    std::vector<api_types::GameHighScore> members;
    for(auto const member : value.get("result",""))
        members.emplace_back(member);

    return members;
}

std::vector<api_types::GameHighScore> tgbot::methods::Api::getGameHighScores(const int &userId,
                                                                             const std::string &inlineMessageId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/getGameHighScores?user_id=" << userId << "&inline_message_id="
            << inlineMessageId;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    std::vector<api_types::GameHighScore> members;
    for(auto const member : value.get("result",""))
        members.emplace_back(member);

    return members;
}

//deleteChatPhoto
bool tgbot::methods::Api::deleteChatPhoto(const std::string &chatId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/deleteChatPhoto?chat_id=" + chatId), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//deleteMessage
bool tgbot::methods::Api::deleteMessage(const std::string &chatId, const std::string &messageId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/deleteMessage?chat_id=" << chatId << "&message_id=" << messageId;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//deleteStickerFromSet
bool tgbot::methods::Api::deleteStickerFromSet(const std::string &sticker) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/deleteStickerFromSet?sticker=" + sticker), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//exportChatInviteLink
std::string tgbot::methods::Api::exportChatInviteLink(const std::string &chatId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/exportChatInviteLink?chat_id=" + chatId), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return value.get("result","").asCString();
}

//kickChatMember
bool tgbot::methods::Api::kickChatMember(const std::string &chatId, const int &userId,
                                         const int &untilDate) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/kickChatMember?chat_id=" << chatId << "&user_id=" << userId;

    if(untilDate != -1)
        url << "&until_date=" << untilDate;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//leaveChat
bool tgbot::methods::Api::leaveChat(const std::string &chatId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::get(inst,baseApi + "/leaveChat?chat_id=" + chatId), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//pinChatMessage
bool tgbot::methods::Api::pinChatMessage(const std::string &chatId, const std::string &messageId,
                                         const bool &disableNotifiation) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/pinChatMessage?chat_id=" << chatId << "&message_id=" << messageId;

    if(disableNotifiation)
        url << "&disable_notification=true";

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//promoteChatMember
bool tgbot::methods::Api::promoteChatMember(const std::string &chatId, const int &userId,
                                            const types::ChatMemberPromote &permissions) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/promoteChatMember?chat_id=" << chatId << "&user_id=" << userId
            << "&can_post_messages=" << BOOL_TOSTR(permissions.canPostMessages)
            << "&can_change_info=" << BOOL_TOSTR(permissions.canChangeInfo)
            << "&can_edit_messages=" << BOOL_TOSTR(permissions.canEditMessages)
            << "&can_delete_messages=" << BOOL_TOSTR(permissions.canDeleteMessages)
            << "&can_invite_users=" << BOOL_TOSTR(permissions.canInviteUsers)
            << "&can_restrict_members=" << BOOL_TOSTR(permissions.canRestrictMembers)
            << "&can_pin_messages=" << BOOL_TOSTR(permissions.canPinMessages)
            << "&can_promote_members=" << BOOL_TOSTR(permissions.canPromoteMembers);

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//restrictChatMember
bool tgbot::methods::Api::restrictChatMember(const std::string &chatId, const int &userId,
                                             const types::ChatMemberRestrict &permissions,
                                             const int &untilDate) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/promoteChatMember?chat_id=" << chatId << "&user_id=" << userId
            << "&can_send_messages=" << BOOL_TOSTR(permissions.canSendMessages)
            << "&can_send_media_messages=" << BOOL_TOSTR(permissions.canSendMediaMessages)
            << "&can_send_other_messages=" << BOOL_TOSTR(permissions.canSendOtherMessages)
            << "&can_add_web_page_previews=" << BOOL_TOSTR(permissions.canAddWebPagePreviews);

    if(untilDate != -1)
        url << "&until_date=" << untilDate;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//unbanChatMember
bool tgbot::methods::Api::unbanChatMember(const std::string &chatId, const int &userId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/unbanChatMember?chat_id=" << chatId << "&user_id=" << userId;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//unpinChatMessage
bool tgbot::methods::Api::unpinChatMessage(const std::string &chatId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/unpinChatMessage?chat_id=" << chatId;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//setChatDescription
bool tgbot::methods::Api::setChatDescription(const std::string &chatId, const std::string &description) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/setChatDescription?chat_id=" << chatId << "&description=" << description;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//setChatTitle
bool tgbot::methods::Api::setChatTitle(const std::string &chatId, const std::string &title) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/setChatTitle?chat_id=" << chatId << "&title=" << title;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//setChatPhoto
bool tgbot::methods::Api::setChatPhoto(const std::string &chatId, const std::string &filename,
                                        const std::string& mimeType) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    reader.parse(http::multiPartUpload(inst,
                                       baseApi + "/setChatPhoto",
                                       chatId,
                                       mimeType,
                                       "photo",
                                       filename), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//setGameScore
api_types::Message tgbot::methods::Api::setGameScore(const std::string &userId, const int &score,
                                                     const int &chatId,
                                                     const int &messageId, const bool &force,
                                                     const bool &disableEditMessage) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/setGameScore?user_id=" << userId << "&score=" << score
            << "&chat_id=" << chatId << "&message_id=" << messageId;

    if(force)
        url << "&force=true";

    if(disableEditMessage)
        url << "&disable_edit_message=true";

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return api_types::Message(value.get("result",""));
}

api_types::Message tgbot::methods::Api::setGameScore(const std::string &userId, const int &score,
                                                     const std::string &inlineMessageId, const bool &force,
                                                     const bool &disableEditMessage) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/setGameScore?user_id=" << userId << "&score=" << score
            << "&inline_message_id=" << inlineMessageId;

    if(force)
        url << "&force=true";

    if(disableEditMessage)
        url << "&disable_edit_message=true";

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return api_types::Message(value.get("result",""));
}

//setStickerPositionInSet
bool tgbot::methods::Api::setStickerPositionInSet(const std::string &sticker, const int position) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/setStickerPositionInSet?sticker=" << sticker << "&position=" << position;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//uploadStickerFile
api_types::File tgbot::methods::Api::uploadStickerFile(const int &userId, const std::string &pngSticker,
                                                       const types::FileSource &source) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    if(source == types::FileSource::EXTERNAL) {
        std::stringstream url;
        url << baseApi << "/uploadStickerFile?user_id=" << userId << "&png_sticker=" << pngSticker;

        reader.parse(http::get(inst,url.str()), value);
    } else
        reader.parse(http::multiPartUpload(inst,
                                           baseApi + "/uploadStickerFile",
                                           userId,
                                           pngSticker), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return api_types::File(value.get("result",""));
}

//addStickerToSet
bool tgbot::methods::Api::addStickerToSet(const int &userId, const std::string &name, const std::string &emoji,
                                          const std::string &pngSticker, const types::FileSource &source) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    if(source == types::FileSource::EXTERNAL) {
        std::stringstream url;
        url << baseApi << "/addStickerToSet?user_id=" << userId << "&png_sticker=" << pngSticker
                << "&name=" << name << "&emoji=" << emoji;

        reader.parse(http::get(inst,url.str()), value);
    } else
        reader.parse(http::multiPartUpload(inst, baseApi + "/addStickerToSet",
                                           userId,name,emoji,pngSticker,""),
                    value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

bool tgbot::methods::Api::addStickerToSet(const int &userId, const std::string &name, const std::string &emoji,
                                          const std::string &pngSticker, const api_types::MaskPosition &maskPosition,
                                          const types::FileSource &source) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    const std::string&& serMaskPosition = toString(maskPosition);

    if(source == types::FileSource::EXTERNAL) {
        std::stringstream url;
        url << baseApi << "/addStickerToSet?user_id=" << userId << "&png_sticker=" << pngSticker
                << "&name=" << name << "&emoji=" << emoji
                << "&mask_position=" << serMaskPosition;

        reader.parse(http::get(inst,url.str()), value);
    } else
        reader.parse(http::multiPartUpload(inst,baseApi + "/addStickerToSet",userId,name,
                                           emoji,serMaskPosition,pngSticker,""),value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//createNewStickerSet
bool tgbot::methods::Api::createNewStickerSet(const int &userId, const std::string &name, const std::string &title,
                                              const std::string &emoji, const std::string &pngSticker,
                                              const types::FileSource &source) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    if(source == types::FileSource::EXTERNAL) {
        std::stringstream url;
        url << baseApi << "/createNewStickerSet?user_id=" << userId << "&png_sticker=" << pngSticker
                << "&name=" << name << "&emoji=" << emoji << "&title=" << title;

        reader.parse(http::get(inst,url.str()), value);
    } else
        reader.parse(http::multiPartUpload(inst, baseApi + "/addStickerToSet",
                                           userId,name,emoji,pngSticker,title), value);

    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

bool tgbot::methods::Api::createNewStickerSet(const int &userId, const std::string &name, const std::string &title,
                                              const std::string &emoji, const std::string &pngSticker,
                                              const api_types::MaskPosition &maskPosition,
                                              const types::FileSource &source) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    const std::string&& serMaskPosition = toString(maskPosition);

    if(source == types::FileSource::EXTERNAL) {
        std::stringstream url;
        url << baseApi << "/createNewStickerSet?user_id=" << userId << "&png_sticker=" << pngSticker
                << "&name=" << name << "&emoji=" << emoji << "&title=" << title
                << "&mask_position=" << serMaskPosition;

        reader.parse(http::get(inst,url.str()), value);
    } else
        reader.parse(http::multiPartUpload(inst,baseApi + "/addStickerToSet",userId,name,
                                           emoji,serMaskPosition,pngSticker,title),value);

    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//answerPreCheckoutQuery
bool tgbot::methods::Api::answerPreCheckoutQuery(const std::string &preCheckoutQueryId) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/answerPreCheckoutQuery?pre_checkout_query_id="
        << preCheckoutQueryId << "&ok=true";

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

bool tgbot::methods::Api::answerPreCheckoutQuery(const std::string &preCheckoutQueryId, const std::string &errorMessage) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/answerPreCheckoutQuery?pre_checkout_query_id="
        << preCheckoutQueryId << "&ok=false" << "&error_message=" << errorMessage;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//answerShippingQuery
bool tgbot::methods::Api::answerShippingQuery(const std::string &shippingQueryId, const std::string &errorMessage) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/answerShippingQuery?shipping_query_id="
        << shippingQueryId << "&ok=false" << "&error_message=" << errorMessage;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

bool tgbot::methods::Api::answerShippingQuery(const std::string &shippingQueryId,
                                              const std::vector<types::ShippingOption> &shippingOptions) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/answerShippingQuery?shipping_query_id="
        << shippingQueryId << "&ok=true&shipping_options=[";

    const size_t& nOptions = shippingOptions.size();
    for(size_t i = 0; i < nOptions;i++) {
        SEPARATE(i, url);
        url << toString(shippingOptions.at(i));
    }

    url << "]";

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//answerCallbackQuery
bool tgbot::methods::Api::answerCallbackQuery(const std::string &callbackQueryId, const std::string &text,
                                              const bool &showAlert, const std::string &url,
                                              const int &cacheTime) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream surl;
    surl << baseApi << "/answerCallbackQuery?callback_query_id=" << callbackQueryId;

    if(text != "")
        surl << "&text=" << text;

    if(showAlert)
        surl << "&show_alert=true";

    if(url != "")
        surl << "&url=" << url;

    if(cacheTime)
        surl << "&cache_time=" << cacheTime;

    reader.parse(http::get(inst,surl.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}

//answerInlineQuery
bool tgbot::methods::Api::answerInlineQuery(const std::string &inlineQueryId,
                                            const std::vector<types::InlineQueryResult> &results,
                                            const int &cacheTime, const bool &isPersonal,
                                            const std::string &nextOffset, const std::string &switchPmText,
                                            const std::string &switchPmParameter) const {
    CURL* inst = http::curlEasyInit();
    Json::Value value;
    Json::Reader reader;

    std::stringstream url;
    url << baseApi << "/answerInlineQuery?inline_query_id=" << inlineQueryId
        << "&results=[";

    const size_t& nResults = results.size();
    for(size_t i = 0; i < nResults;i++) {
        SEPARATE(i, url);
        url << results.at(i).toString();
    }

    url << "]";

    if(cacheTime)
        url << "&cache_time=" << cacheTime;

    if(isPersonal)
        url << "&is_personal=true";

    if(nextOffset != "")
        url << "&next_offset=" << nextOffset;

    if(switchPmText != "")
        url << "&switch_pm_text=" << switchPmText;

    if(switchPmParameter != "")
        url << "&switch_pm_parameter=" << switchPmParameter;

    reader.parse(http::get(inst,url.str()), value);
    curl_easy_cleanup(inst);

    if(!value.get("ok","").asBool())
        throw TelegramException(value.get("description","").asCString());

    return true;
}



