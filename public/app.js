const connect = (url) => {
  const socket = new WebSocket(url);

  socket.onopen = (event) => {
    console.log("Connection established successfully", event);
  };

  socket.onmessage = (event) => {
    console.log("Server says:", event.data.length, event.data);
  };

  socket.onclose = (event) => {
    console.log("onclose", event);
    setTimeout(() => {
      console.log("Reconnecting...");
      connect(url);
    }, 1000);
  };

  socket.onerror = (event) => {
    console.error("Socket error", event);
    socket.close();
  };

  const btn = document.getElementById("btn-send-msg");
  btn.addEventListener("click", (event) => {
    const msg = document.querySelector("input").value || 0;
    socket.send("a".repeat(msg));
  });
};

connect("ws://localhost:3001");
