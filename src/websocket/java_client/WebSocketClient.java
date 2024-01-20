import javax.websocket.*;
import java.net.URI;
import java.util.Scanner;

@ClientEndpoint
public class WebSocketClient {
    @OnMessage
    public void onMessage(String message) {
        System.out.println("Received message from server: " + message);
    }

    public static void main(String[] args) {
        // String uri = "ws://172.30.9.34:7681"; // 서버 endpoint에 "/time" 추가
        String uri = "ws://172.30.9.34:8080"; // 서버 endpoint에 "/time" 추가
        WebSocketContainer container = ContainerProvider.getWebSocketContainer();

        try {
            container.connectToServer(WebSocketClient.class, URI.create(uri));

            // 클라이언트에서 주기적으로 서버에게 현재 시간을 요청
            while (true) {
                Session session = container.connectToServer(WebSocketClient.class, URI.create(uri));
                session.getBasicRemote().sendText("time");
                Thread.sleep(1000); // 5초마다 반복
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
