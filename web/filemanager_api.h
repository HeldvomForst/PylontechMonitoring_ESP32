#pragma once
#include <WebServer.h>
#include <SPIFFS.h>
#include "filemanager.h"

void registerFileManagerAPI(WebServer &server) {

    // ---------------------------------------------------------
    // HTML-Seite (RAM-frei, PROGMEM-Streaming)
    // ---------------------------------------------------------
    server.on("/filemanager", HTTP_GET, [&]() {
        server.setContentLength(strlen_P(FILEMANAGER_PAGE));
        server.send(200, "text/html", "");
        server.sendContent_P(FILEMANAGER_PAGE);
        server.sendContent("");
    });

    // ---------------------------------------------------------
    // Directory Listing (Streaming JSON)
    // ---------------------------------------------------------
    server.on("/fm/list", HTTP_GET, [&]() {
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "application/json", "");

        server.sendContent("[");

        File root = SPIFFS.open("/");
        File file = root.openNextFile();
        bool first = true;

        while (file) {
            if (!first) server.sendContent(",");
            first = false;

            server.sendContent("{\"name\":\"");
            server.sendContent(file.name());
            server.sendContent("\",\"size\":");
            server.sendContent(String(file.size()));
            server.sendContent("}");

            file = root.openNextFile();
        }

        server.sendContent("]");
        server.sendContent("");
    });

    // ---------------------------------------------------------
    // File Download (perfekt, streamFile)
    // ---------------------------------------------------------
    server.on("/fm/download", HTTP_GET, [&]() {
        if (!server.hasArg("file")) {
            server.send(400, "text/plain", "Missing file");
            return;
        }

        String path = server.arg("file");
        File f = SPIFFS.open(path, "r");
        if (!f) {
            server.send(404, "text/plain", "File not found");
            return;
        }

        server.streamFile(f, "application/octet-stream");
        f.close();
    });

	// ---------------------------------------------------------
	// File Delete (korrigiert)
	// ---------------------------------------------------------
	server.on("/fm/delete", HTTP_GET, [&]() {
		if (!server.hasArg("file")) {
			server.send(400, "text/plain", "Missing file");
			return;
		}

		String path = server.arg("file");

		// WICHTIG: führenden Slash erzwingen
		if (!path.startsWith("/")) {
			path = "/" + path;
		}

		if (!SPIFFS.exists(path)) {
			server.send(404, "text/plain", "File not found: " + path);
			return;
		}

		SPIFFS.remove(path);
		server.send(200, "text/plain", "OK");
	});


    // ---------------------------------------------------------
    // File Upload (RAM-sicher)
    // ---------------------------------------------------------
    server.on("/fm/upload", HTTP_POST,
        [&]() { server.send(200, "text/plain", "OK"); },
        [&]() {
            HTTPUpload &up = server.upload();
            static File uploadFile;

            if (up.status == UPLOAD_FILE_START) {
                String filename = "/" + up.filename;
                uploadFile = SPIFFS.open(filename, "w");
            }
            else if (up.status == UPLOAD_FILE_WRITE) {
                if (uploadFile) uploadFile.write(up.buf, up.currentSize);
            }
            else if (up.status == UPLOAD_FILE_END) {
                if (uploadFile) uploadFile.close();
            }
        }
    );
}