
#include "CrealityrintDump.hpp"
// 自定义对话框类







void ErrorReportDialog::sendEmail(wxString zipFilePath) {
        // 发送邮件
        CURL *curl;
        CURLcode res = CURLE_OK;
        struct curl_slist *recipients = NULL;
        struct curl_slist*   headerlist = NULL;
        //struct upload_status upload_ctx = { 0 };
        /* Time */
        time_t     rawtime;
        struct tm* timeinfo;
        char       time_buffer[128];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(time_buffer, 128, "%a, %d %b %Y %H:%M:%S %z", timeinfo);
        const char payload_template[] = "Date: %s\r\n"
                                        "To: %s\r\n"
                                        "From: %s\r\n"
                                        "Message-ID: <%s>\r\n"
                                        "Subject: %s\r\n"
                                        "\r\n";
        char*      from               = DUMPTOOL_USER;
        char*      to                 = DUMPTOOL_TO;
        char* message_id = from;
        char*  subject          = "Error Report"; 
        size_t payload_text_len = strlen(payload_template) + strlen(time_buffer) + strlen(to) + strlen(from) + strlen(subject) +
                                  strlen(message_id) + 1;

        char* payload_text = (char*) malloc(payload_text_len);
        memset(payload_text, 0, payload_text_len);
        sprintf(payload_text, payload_template, time_buffer, to, from, message_id, subject);
        curl = curl_easy_init();
        curl_version_info_data* ver = curl_version_info(CURLVERSION_NOW);

         if(curl) {
             curl_mime* mime = curl_mime_init(curl);

             // 设置正文部分
             curl_mimepart* part = curl_mime_addpart(mime);
             curl_mime_data(part, "This is an error report.", CURL_ZERO_TERMINATED);
             curl_mime_type(part, "text/plain");


             // 设置附件部分
             part = curl_mime_addpart(mime);
             curl_mime_filedata(part, zipFilePath.ToStdString().c_str()); // 替换为你的文件路径
             curl_mime_name(part, "dumpinfo.zip");               // 设置附件名称
             curl_mime_filename(part, "dumpinfo.zip");           // 设置附件在邮件中的显示名称
             
             headerlist = curl_slist_append(headerlist, payload_text);
             curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from);
             recipients = curl_slist_append(recipients, to);
             curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
             curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
             curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
             curl_easy_setopt(curl, CURLOPT_URL, DUMPTOOL_HOST);
             curl_easy_setopt(curl, CURLOPT_USERNAME, DUMPTOOL_USER);
            curl_easy_setopt(curl, CURLOPT_PASSWORD, DUMPTOOL_PASS);
            
            
            
            
         
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            //curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        
            
            
            res = curl_easy_perform(curl);

            if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s", curl_easy_strerror(res));

            curl_slist_free_all(recipients);
            curl_mime_free(mime);
            curl_easy_cleanup(curl);
            
        }

        return ;
    }
    wxString ErrorReportDialog::zipFiles() {
        // 创建一个zip文件
        wxString format1 = "%Y%m%d%H%M%S";
        wxString zipFilePath = wxString::Format("%s/CrealityPrint_%s_%s.zip",wxFileName::GetTempDir(),CREALITYPRINT_VERSION,wxDateTime::Now().Format(format1));
        mz_zip_archive archive;
        mz_zip_zero_struct(&archive);
        mz_bool status = mz_zip_writer_init_file(&archive, zipFilePath.mb_str(), 0);
        if (!status) {
            std::cerr << "Failed to create zip file!" << std::endl;
            return "";
        }
        // 添加文件到zip文件
        wxFileName fileName(m_dumpFilePath);
        wxString nameWithExt = fileName.GetFullName();
        status = mz_zip_writer_add_file(&archive, nameWithExt.mb_str(), m_dumpFilePath.mb_str(), "", 0, MZ_BEST_COMPRESSION);
        if (!status) {
            std::cerr << "Failed to add file to zip!" << std::endl;
            mz_zip_writer_end(&archive);
            return "";
        }
         status = mz_zip_writer_add_file(&archive, "system_info.json", m_systemInfoFilePath.mb_str(), "", 0, MZ_BEST_COMPRESSION);
        if (!status) {
            std::cerr << "Failed to add file to zip!" << std::endl;
            mz_zip_writer_end(&archive);
            return "";
        }

        status = mz_zip_writer_finalize_archive(&archive);
        if (MZ_FALSE == status) {
            mz_zip_writer_end(&archive);
            return "";
        }
        // 关闭zip文件
        mz_zip_writer_end(&archive);
        return zipFilePath;
    }

    void ErrorReportDialog::sendReport()
    {
         m_systemInfoFilePath = getSystemInfo();
        if(!m_dumpFilePath.IsEmpty() && !m_systemInfoFilePath.IsEmpty()) {
            wxString dumpfile = zipFiles();
            if(dumpfile.IsEmpty()) {
                    return ;
            }
            sendEmail(dumpfile);
        }
    }
// 自定义应用程序类
