package dev.nothingbrowser.bridge;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

import java.util.Map;

public class Main {

    static final Gson GSON = new GsonBuilder().disableHtmlEscaping().create();

    public static void main(String[] args) {
        if (args.length < 2) {
            error("Usage: newpipe-bridge.jar <command> <query>");
            error("Commands: search <query>  |  info <url>  |  download <url> <format> <outdir>");
            System.exit(1);
        }

        String cmd = args[0];

        try {
            // Init NewPipe's downloader (required before any extraction)
            NewPipeInit.init();

            switch (cmd) {
                case "search":
                    SearchCommand.run(args[1]);
                    break;
                case "info":
                    InfoCommand.run(args[1]);
                    break;
                case "download":
                    if (args.length < 4) {
                        error("download requires: <url> <streamUrl> <outPath>");
                        System.exit(1);
                    }
                    DownloadCommand.run(args[1], args[2], args[3]);
                    break;
                default:
                    error("Unknown command: " + cmd);
                    System.exit(1);
            }
        } catch (Exception e) {
            System.out.println(GSON.toJson(Map.of(
                "error", true,
                "message", e.getMessage() != null ? e.getMessage() : e.getClass().getSimpleName()
            )));
            System.exit(1);
        }
    }

    static void error(String msg) {
        System.err.println("[newpipe-bridge] " + msg);
    }
}
