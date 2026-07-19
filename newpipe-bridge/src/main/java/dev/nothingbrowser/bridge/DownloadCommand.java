package dev.nothingbrowser.bridge;

import java.io.*;
import java.net.*;
import java.nio.file.*;
import java.util.Map;

public class DownloadCommand {

    // Qt reads stdout line by line looking for:
    //   {"progress": 45}       → progress update
    //   {"done": true, "path": "/path/to/file"}  → finished
    //   {"error": true, "message": "..."}         → failed

    public static void run(String videoUrl, String streamUrl, String outPath) throws Exception {
        Path out = Paths.get(outPath);
        Files.createDirectories(out.getParent() == null ? Paths.get(".") : out.getParent());

        URL url = new URL(streamUrl);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setRequestMethod("GET");
        conn.setConnectTimeout(15_000);
        conn.setReadTimeout(30_000);
        conn.setRequestProperty("User-Agent",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
        conn.setRequestProperty("Referer", "https://www.youtube.com/");
        conn.setInstanceFollowRedirects(true);
        conn.connect();

        long total  = conn.getContentLengthLong();
        long downloaded = 0;
        int  lastReported = -1;

        try (InputStream in  = conn.getInputStream();
             OutputStream file = Files.newOutputStream(out)) {
            byte[] buf = new byte[64 * 1024]; // 64KB chunks
            int n;
            while ((n = in.read(buf)) != -1) {
                file.write(buf, 0, n);
                downloaded += n;
                if (total > 0) {
                    int pct = (int)(downloaded * 100L / total);
                    if (pct != lastReported) {
                        lastReported = pct;
                        System.out.println(Main.GSON.toJson(Map.of("progress", pct)));
                        System.out.flush();
                    }
                }
            }
        }

        System.out.println(Main.GSON.toJson(Map.of(
            "done", true,
            "path", out.toAbsolutePath().toString(),
            "bytes", downloaded
        )));
        System.out.flush();
    }
}
