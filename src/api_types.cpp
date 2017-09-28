#include <sstream>
#include <tgbot/methods/types.h>

#define BOOL_TOSTR(xvalue) \
	((xvalue) ? "true" : "false")

std::string tgbot::methods::types::EmptyReplyMarkup::toString() const {
	return "";
}

std::string tgbot::methods::types::InlineKeyboardMarkup::toString() const {
	const int& r = inlineKeyboard.getMaxRows();
	const int& c = inlineKeyboard.getMaxCols();

	if(!r || !c)
		return "";

	std::stringstream jsonify;
	jsonify << "{ \"inline_keyboard\": [";

	for(int i=0;i<r;i++) { 
		jsonify << "[";
		for(int j=0;j<c;j++) {
			const InlineKeyboardButton& currentButton = inlineKeyboard.at(i,j);
			jsonify << "{ \"text\": \"" << currentButton.text 
				<< "\", \"pay\": " << BOOL_TOSTR(currentButton.pay);

			if(currentButton.url)
				jsonify << ", \"url\": \"" << *(currentButton.url.get()) << "\"";

			if(currentButton.callbackData)
				jsonify << ", \"callback_data\": \"" << *(currentButton.callbackData.get()) << "\"";

			if(currentButton.switchInlineQuery)
				jsonify << ", \"switch_inline_query\": \"" << *(currentButton.switchInlineQuery.get()) << "\"";

			if(currentButton.switchInlineQueryCurrentChat)
				jsonify << ", \"switch_inline_query_current_chat\": \"" << *(currentButton.switchInlineQueryCurrentChat.get()) << "\"";

			jsonify << "}";
		}

		jsonify << "]";
	}

	jsonify << "]";

	return jsonify.str();
}

std::string tgbot::methods::types::ReplyKeyboardMarkup::toString() const {
	const int& r = keyboard.getMaxRows();
	const int& c = keyboard.getMaxCols();
	 
	if(!r || !c)
		return "";

	std::stringstream jsonify;
	jsonify << "[";

	for(int i=0;i<r;i++) {
		jsonify << "[";
		for(int j=0;j<c;j++) {
			const KeyboardButton& currentButton = keyboard.at(i,j);
			jsonify << "{ \"text\": \"" << currentButton.text << "\","
				<< "\"request_contact\": " << BOOL_TOSTR(currentButton.requestContact)
				<< ",\"request_location\": " << BOOL_TOSTR(currentButton.requestLocation) 
				<< "}";
		}
		jsonify << "]";
	}

	jsonify << "]";
	
	return jsonify.str();
}

std::string tgbot::methods::types::ReplyKeyboardRemove::toString() const {
	std::stringstream jsonify;
	jsonify << "{\"remove_keyboard\": true, \"selective\": " 
		<< BOOL_TOSTR(selective) 
		<< "}";
	
	return jsonify.str();
}

std::string tgbot::methods::types::ForceReply::toString() const {
	std::stringstream jsonify;
	jsonify << "{\"force_reply\": true, \"selective\": " 
		<< BOOL_TOSTR(selective) 
		<< "}";
	
	return jsonify.str();
}