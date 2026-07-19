package dev.nothingbrowser.bridge;

import org.schabi.newpipe.extractor.NewPipe;
import org.schabi.newpipe.extractor.downloader.Downloader;
import org.schabi.newpipe.extractor.downloader.Request;
import org.schabi.newpipe.extractor.downloader.Response;
import org.schabi.newpipe.extractor.exceptions.ReCaptchaException;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class NewPipeInit {

    public static void init() {
        NewPipe.init(new Downloader() {
            @Override
            public Response execute(Request request) throws IOException, ReCaptchaException {
                HttpURLConnection conn = (HttpURLConnection)
                    new URL(request.url()).openConnection();

                conn.setRequestMethod(request.httpMethod());
                conn.setConnectTimeout(15_000);
                conn.setReadTimeout(15_000);
                conn.setInstanceFollowRedirects(true);

                // Default headers
                conn.setRequestProperty("User-Agent",
                    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                    + "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
                conn.setRequestProperty("Accept-Language", "en-US,en;q=0.9");
                conn.setRequestProperty("Cookie",
                "CONSENT=YES+; SOCS=CAESEwgDEgk0ODA3Nzg3MjQaAmVuIAEaBgiA_LyaBg");

                // Custom headers from request
                if (request.headers() != null) {
                    request.headers().forEach((k, vals) -> {
                        if (vals != null && !vals.isEmpty())
                            conn.setRequestProperty(k, vals.get(0));
                    });
                }

                // Body
                if (request.dataToSend() != null) {
                    conn.setDoOutput(true);
                    conn.getOutputStream().write(request.dataToSend());
                }

                int code = conn.getResponseCode();

                // Read response
                byte[] body;
                try {
                    body = conn.getInputStream().readAllBytes();
                } catch (IOException e) {
                    body = new byte[0];
                }

                // Collect response headers
                Map<String, List<String>> headers = new HashMap<>(conn.getHeaderFields());
                headers.remove(null); // remove status line entry

                String latestUrl = conn.getURL().toString();
                return new Response(code, conn.getResponseMessage(),
                    headers, new String(body), latestUrl);
            }
        });
    }
}
