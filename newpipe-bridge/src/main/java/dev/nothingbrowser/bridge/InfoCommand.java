package dev.nothingbrowser.bridge;

import org.schabi.newpipe.extractor.ServiceList;
import org.schabi.newpipe.extractor.stream.*;

import java.util.*;

public class InfoCommand {

    public static void run(String url) throws Exception {
        StreamInfo info = StreamInfo.getInfo(ServiceList.YouTube, url);

        Map<String, Object> result = new LinkedHashMap<>();
        result.put("title",      info.getName());
        result.put("uploader",   info.getUploaderName());
        result.put("duration",   info.getDuration());
        result.put("views",      info.getViewCount());
        result.put("likes",      info.getLikeCount());
        result.put("description", info.getDescription().getContent());
        result.put("thumbnail",  info.getThumbnails().isEmpty()
            ? "" : info.getThumbnails().get(0).getUrl());

        // Video+Audio streams (muxed)
        List<Map<String, Object>> muxed = new ArrayList<>();
        for (VideoStream s : info.getVideoStreams()) {
            Map<String, Object> m = new LinkedHashMap<>();
            m.put("type",     "muxed");
            m.put("quality",  s.getResolution());
            m.put("format",   s.getFormat() != null ? s.getFormat().getName() : "mp4");
            m.put("url",      s.getContent());
            m.put("fps",      s.getFps());
            muxed.add(m);
        }

        // Video-only streams (higher quality, no audio)
        List<Map<String, Object>> videoOnly = new ArrayList<>();
        for (VideoStream s : info.getVideoOnlyStreams()) {
            Map<String, Object> m = new LinkedHashMap<>();
            m.put("type",     "video-only");
            m.put("quality",  s.getResolution());
            m.put("format",   s.getFormat() != null ? s.getFormat().getName() : "webm");
            m.put("url",      s.getContent());
            m.put("fps",      s.getFps());
            videoOnly.add(m);
        }

        // Audio-only streams
        List<Map<String, Object>> audio = new ArrayList<>();
        for (AudioStream s : info.getAudioStreams()) {
            Map<String, Object> m = new LinkedHashMap<>();
            m.put("type",     "audio");
            m.put("bitrate",  s.getAverageBitrate());
            m.put("format",   s.getFormat() != null ? s.getFormat().getName() : "m4a");
            m.put("url",      s.getContent());
            audio.add(m);
        }

        // Sort by quality (best first)
        muxed.sort((a, b) -> qualityRank((String)b.get("quality"))
                            - qualityRank((String)a.get("quality")));
        videoOnly.sort((a, b) -> qualityRank((String)b.get("quality"))
                               - qualityRank((String)a.get("quality")));
        audio.sort((a, b) -> (int)((Integer)b.get("bitrate") - (Integer)a.get("bitrate")));

        result.put("streams",    muxed);
        result.put("videoOnly",  videoOnly);
        result.put("audio",      audio);

        System.out.println(Main.GSON.toJson(result));
    }

    private static int qualityRank(String q) {
        if (q == null) return 0;
        if (q.contains("2160") || q.contains("4K")) return 2160;
        if (q.contains("1440"))  return 1440;
        if (q.contains("1080"))  return 1080;
        if (q.contains("720"))   return 720;
        if (q.contains("480"))   return 480;
        if (q.contains("360"))   return 360;
        if (q.contains("240"))   return 240;
        if (q.contains("144"))   return 144;
        return 0;
    }
}
