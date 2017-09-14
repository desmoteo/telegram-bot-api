#ifndef TGBOT_HTTPS_H
#define TGBOT_HTTPS_H

#include <curl/curl.h>
#include <string>
#include <atomic>

namespace tgbot {

	namespace utils {

		/*!
		 * @brief Returned by Https::get() and Https::multiPartUpload()
		 */
		struct HttpResponse {
			std::string body;
			int code;
		};

		/*!
		 * @brief This is meant to internal usage, but user can always use it
		 */
		class Https {
			public:
				Https();
				~Https();

				HttpResponse get(const std::string& fullUrl);
				HttpResponse multiPartUpload(const std::string& fullUrl,
						const std::string& localFileName,
						std::ifstream& stream);
			private:
				std::atomic<CURL*> curlinst;
		};

	} //utils

} //tgbot

#endif