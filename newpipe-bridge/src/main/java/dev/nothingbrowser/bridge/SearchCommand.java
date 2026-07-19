package dev.nothingbrowser.bridge;

import org.schabi.newpipe.extractor.ServiceList;
import org.schabi.newpipe.extractor.search.SearchExtractor;
import org.schabi.newpipe.extractor.stream.StreamInfoItem;
import org.schabi.newpipe.extractor.InfoItem;
import org.schabi.newpipe.extractor.ListExtractor;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.LinkedHashMap;

public class SearchCommand {

    public static void run(String query) throws Exception {
        SearchExtractor extractor = ServiceList.YouTube
            .getSearchExtractor(query);
        extractor.fetchPage();

        ListExtractor.InfoItemsPage<InfoItem> page = extractor.getInitialPage();
        List<Map<String, Object>> results = new ArrayList<>();

        for (InfoItem item : page.getItems()) {
            if (!(item instanceof StreamInfoItem)) continue;
            StreamInfoItem stream = (StreamInfoItem) item;

            Map<String, Object> r = new LinkedHashMap<>();
            r.put("id",        extractVideoId(stream.getUrl()));
            r.put("url",       stream.getUrl());
            r.put("title",     stream.getName());
            r.put("uploader",  stream.getUploaderName());
            r.put("duration",  stream.getDuration());  // seconds, -1 if live
            r.put("views",     stream.getViewCount());
            r.put("thumbnail", stream.getThumbnails().isEmpty()
                ? "" : stream.getThumbnails().get(0).getUrl());
            r.put("uploaded", stream.getUploadDate() != null
                ? stream.getUploadDate().offsetDateTime().toString() : "");
            results.add(r);
        }

        System.out.println(Main.GSON.toJson(results));
    }

    private static String extractVideoId(String url) {
        // handles both /watch?v=ID and /shorts/ID
        if (url.contains("v=")) {
            String id = url.substring(url.indexOf("v=") + 2);
            int amp = id.indexOf('&');
            return amp > 0 ? id.substring(0, amp) : id;
        }
        String[] parts = url.split("/");
        return parts[parts.length - 1];
    }
}
