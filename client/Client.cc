//
// Created by hbeckeri on 4/23/15.
//

#include <stdio.h>
#include <gtk/gtk.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#define MAX_RESPONSE (10 * 1024)

GtkListStore * list_rooms;
GtkListStore * list_users;
GtkWidget *roomTreeView;
GtkWidget *userTreeView;
GtkWidget *window;
GtkWindow *mainWindow;
GtkWidget *statusLabel;
GtkWidget *messages;
GtkWidget *myMessage;
GtkTextIter start_find;
GtkTextIter end_find;
GtkTextBuffer *sendBuffer;
GtkTextBuffer *receiveBuffer;
GtkAdjustment * bottom;

char * user;
char * password;
char * room;
char * host;
int port = 5000;
char * portString;
char * status;
struct SOCKET {
    GtkWidget *host;
    GtkWidget *port;
};
struct CREDS {
    GtkWidget *username;
    GtkWidget *password;
};
char * response;
char * messageResponse;
CREDS * credentials = (CREDS *) malloc(sizeof(CREDS*) * 1);
s
//---------------------NETWORK STUFF-------------------------------------//
int open_client_socket(char * host, int port) {
    // Initialize socket address structure
    struct sockaddr_in socketAddress;

    // Clear sockaddr structure
    memset((char *) &socketAddress, 0, sizeof(socketAddress));

    // Set family to Internet
    socketAddress.sin_family = AF_INET;

    // Set port
    socketAddress.sin_port = htons((u_short) port);

    // Get host table entry for this host
    struct hostent *ptrh = gethostbyname(host);
    if (ptrh == NULL) {
        perror("gethostbyname");
        //exit(1);
    }

    // Copy the host ip address to socket address structure
    memcpy(&socketAddress.sin_addr, ptrh->h_addr, ptrh->h_length);

    // Get TCP transport protocol entry
    struct protoent *ptrp = getprotobyname("tcp");
    if (ptrp == NULL) {
        perror("getprotobyname");
        //exit(1);
    }

    // Create a tcp socket
    int sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
    if (sock < 0) {
        perror("socket");
        //exit(1);
    }

    // Connect the socket to the specified server
    if (connect(sock, (struct sockaddr *) &socketAddress,
                sizeof(socketAddress)) < 0) {
        perror("connect");
        //exit(1);
    }

    return sock;
}
int sendCommand(char *  host, int port, char * command, char * response) {

    int sock = open_client_socket( host, port);

    if (sock<0) {
        return 0;
    }

    // Send command
    write(sock, command, strlen(command));
    write(sock, "\r\n",2);

    //Print copy to stdout
    write(1, command, strlen(command));
    write(1, "\r\n",2);

    // Keep reading until connection is closed or MAX_REPONSE
    int n = 0;
    int len = 0;
    while ((n=read(sock, response+len, MAX_RESPONSE - len))>0) {
        len += n;
    }
    response[len]=0;

    close(sock);

    return 1;
}

//---------------------------GUI-------------------------------------//
static void enter_callback( GtkWidget *widget, GtkWidget *entry ) {
    const gchar *entry_text;
    entry_text = gtk_entry_get_text (GTK_ENTRY (entry));
    printf ("Entry contents: %s\n", entry_text);
}
static void updateSocket(SOCKET * socket) {
    port = atoi((const char *) gtk_entry_get_text (GTK_ENTRY (socket->port)));
    strcpy(host, (char *) gtk_entry_get_text (GTK_ENTRY (socket->host)));

    printf("host: %s port: %d\n", host, port);
}
static GtkWidget *create_list_room(const char * titleColumn, GtkListStore *model) {
    GtkWidget *scrolled_window;
    GtkWidget *tree_view;
    //GtkListStore *model;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    int i;

    /* Create a new scrolled window, with scrollbars only if needed */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    //model = gtk_list_store_new (1, G_TYPE_STRING);
    tree_view = gtk_tree_view_new ();
    roomTreeView = tree_view;
    gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
    gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (model));
    gtk_widget_show (tree_view);

    cell = gtk_cell_renderer_text_new ();

    column = gtk_tree_view_column_new_with_attributes (titleColumn, cell, "text", 0, NULL);

    gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));

    return scrolled_window;
}
static GtkWidget *create_list_user( const char * titleColumn, GtkListStore *model ) {
    GtkWidget *scrolled_window;
    GtkWidget *tree_view;
    //GtkListStore *model;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    int i;

    /* Create a new scrolled window, with scrollbars only if needed */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    //model = gtk_list_store_new (1, G_TYPE_STRING);
    tree_view = gtk_tree_view_new ();
    userTreeView = tree_view;
    gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
    gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (model));
    gtk_widget_show (tree_view);

    cell = gtk_cell_renderer_text_new ();

    column = gtk_tree_view_column_new_with_attributes (titleColumn, cell, "text", 0, NULL);

    gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));

    return scrolled_window;
}
static void insert_text( GtkTextBuffer *buffer, const char * initialText ) {
    GtkTextIter iter;

    gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
    gtk_text_buffer_insert (buffer, &iter, initialText,-1);
}
static GtkWidget *create_text_send( const char * initialText ) {
    GtkWidget *scrolled_window;
    GtkWidget *view;

    view = gtk_text_view_new ();
    sendBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    gtk_container_add (GTK_CONTAINER (scrolled_window), view);
    insert_text (sendBuffer, initialText);

    gtk_widget_show_all (scrolled_window);

    return scrolled_window;
}
static GtkWidget *create_text_receive( const char * initialText ) {
    GtkWidget *scrolled_window;
    GtkWidget *view;

    view = gtk_text_view_new ();
    receiveBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_add (GTK_CONTAINER (scrolled_window), view);
    insert_text (receiveBuffer, initialText);

    gtk_widget_show_all (scrolled_window);

    return scrolled_window;
}

//-------------------------ROOMS-----------------------------------------//
static void createRoom(GtkWidget *roomEntry) {
    char * newRoom = (char *) malloc(sizeof(char *) * 100);
    strcpy(newRoom, (char *) gtk_entry_get_text(GTK_ENTRY(roomEntry)));
    char * msg = (char*) malloc(sizeof(char*) * 100);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "CREATE-ROOM %s %s %s", user, password, newRoom);
    sendCommand(host, port, msg, response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    free(msg);
    msg = (char*) malloc(sizeof(char*) * 100);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "ENTER-ROOM %s %s %s", user, password, newRoom);
    sendCommand(host, port, msg, response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    free(msg);
    msg = (char*) malloc(sizeof(char*) * 100);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "SEND-MESSAGE %s %s %s CREATED-ROOM: \"%s\"", user, password, newRoom, newRoom);
    sendCommand(host, port, msg, response);
    sendCommand(host, port, msg, response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    free(msg);
    msg = (char*) malloc(sizeof(char*) * 100);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "LEAVE-ROOM %s %s %s", user, password, newRoom);
    sendCommand(host, port, msg, response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    free(msg);
    free(newRoom);
}
void update_list_rooms() {
    GtkTreeIter iter;
    GtkTreeIter removeIter;
    int i = 1;
    char * j;
    char * word;
    char * msg = (char *) malloc(sizeof(char *) * 1000);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "LIST-ROOMS %s %s", user, password);
    sendCommand(host,port,msg,response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    /* Add some messages to the window */
    j = response;
    word = response;
    GtkTreeModel * model = gtk_tree_view_get_model(GTK_TREE_VIEW(roomTreeView));
    GtkTreePath * path = gtk_tree_path_new_first();
    gtk_tree_model_get_iter(model, &removeIter, path);

    while (gtk_list_store_remove(GTK_LIST_STORE (list_rooms), &removeIter)) {}

    while (*j != '\0') {
        if (*(j + 1) == '\r') {
            *(j + 1) = 0;
            msg = g_strdup_printf ("%s", word);
            i++;
            gtk_list_store_append (GTK_LIST_STORE (list_rooms), &iter);
            gtk_list_store_set (GTK_LIST_STORE (list_rooms), &iter, 0, msg, -1);
            g_free (msg);
            j += 3;
            word = j;
        }
        j++;
    }
}
void getRoomSelection() {
    GtkTreeSelection *selection;
    GtkTreeModel     *model;
    GtkTreeIter       iter;

    /* This will only work in single or browse selection mode! */

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(roomTreeView));
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get (model, &iter, 0, &room, -1);
        //printf("selected row is: %s\n", room);
    }
    else {
        printf ("no row selected.\n");
    }
}
void leaveRoom() {
    char * msg = (char*) malloc(sizeof(char*) * 100);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "SEND-MESSAGE %s %s %s LEFT ROOM: \"%s\"", user, password, room, room);
    sendCommand(host, port, msg, response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    free(msg);
    msg = (char*) malloc(sizeof(char*) * 100);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "LEAVE-ROOM %s %s %s", user, password, room);
    sendCommand(host, port, msg, response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    free(msg);
}
void enterRoom() {
    leaveRoom();
    getRoomSelection();
    char * msg = (char*) malloc(sizeof(char*) * 100);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "ENTER-ROOM %s %s %s", user, password, room);
    sendCommand(host, port, msg, response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    free(msg);
    msg = (char*) malloc(sizeof(char*) * 100);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "SEND-MESSAGE %s %s %s ENTERED ROOM: \"%s\"", user, password, room, room);
    sendCommand(host, port, msg, response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    free(msg);
}

//--------------------------USERS---------------------------------------//
static void addUser(CREDS * credentials) {
    strcpy(user, (char *) gtk_entry_get_text (GTK_ENTRY (credentials->username)));
    strcpy(password, (char *) gtk_entry_get_text (GTK_ENTRY (credentials->password)));
    char * msg = (char*) malloc(sizeof(char*) * 100);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "ADD-USER %s %s", user, password);
    sendCommand(host, port, msg, response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    free(msg);
}
void update_list_users() {
    GtkTreeIter iter;
    GtkTreeIter removeIter;
    int i = 1;
    char * j;
    char * word;
    char * msg = (char *) malloc(sizeof(char *) * 1000);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    getRoomSelection();
    sprintf(msg, "GET-USERS-IN-ROOM %s %s %s", user, password, room);
    sendCommand(host,port,msg,response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    /* Add some messages to the window */
    j = response;
    word = response;

    GtkTreeModel * model = gtk_tree_view_get_model(GTK_TREE_VIEW(userTreeView));
    GtkTreePath * path = gtk_tree_path_new_first();
    gtk_tree_model_get_iter(model, &removeIter, path);
    while (gtk_list_store_remove(GTK_LIST_STORE (list_users), &removeIter)) {}

    while (*j != '\0') {
        if (*(j + 1) == '\r') {
            *(j + 1) = 0;
            msg = g_strdup_printf ("%s", word);
            i++;
            //printf("-----start-----%s-----end-----\n", word);
            gtk_list_store_append (GTK_LIST_STORE (list_users), &iter);
            gtk_list_store_set (GTK_LIST_STORE (list_users), &iter, 0, msg, -1);
            g_free (msg);
            j += 3;
            word = j;
        }
        j++;
    }
}

//--------------------------MESSAGES-----------------------------------//
static void sendMessage() {
    char * msg = (char*) malloc(sizeof(char*) * 1000);
    char * message = (char*) malloc(sizeof(char*) * 1000);
    gtk_text_buffer_get_start_iter(sendBuffer, &start_find);
    gtk_text_buffer_get_end_iter(sendBuffer, &end_find);
    strcpy(message, gtk_text_buffer_get_text(sendBuffer, &start_find, &end_find, FALSE));
    gtk_text_buffer_delete(sendBuffer, &start_find, &end_find);
    memset(&response[0], 0, sizeof(response));
    printf("*****COMMAND******\n");
    sprintf(msg, "SEND-MESSAGE %s %s %s %s", user, password, room, message);
    sendCommand(host, port, msg, response);
    printf("*****RESPONSE*****\n%s******************\n", response);
    free(msg);
}
static void getMessages() {
    char * msg = (char*) malloc(sizeof(char*) * 1000);
    memset(&messageResponse[0], 0, sizeof(messageResponse));
    printf("*****COMMAND******\n");
    sprintf(msg, "GET-MESSAGES %s %s 0 %s", user, password, room);
    sendCommand(host, port, msg, messageResponse);
    printf("*****RESPONSE*****\n%s******************\n", messageResponse);
    free(msg);

    gtk_text_buffer_get_start_iter(receiveBuffer, &start_find);
    gtk_text_buffer_get_end_iter(receiveBuffer, &end_find);
    gtk_text_buffer_delete(receiveBuffer, &start_find, &end_find);
    insert_text(receiveBuffer, messageResponse);
    bottom = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 10000.0, 0.0, 0.0, 0.0, 0.0));
    gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(messages), bottom);
}

//-------------------------POPUPS-----------------------------------------//
static void loginModal() {
    GtkWidget *userWindow;
    GtkWidget *usernameEntry;
    GtkWidget *passwordEntry;
    GtkWidget *usernameLabel;
    GtkWidget *passwordLabel;
    GtkWidget *vbox, *hbox;
    gint tmp_pos;
    GtkWidget *button;

    //create a new window
    userWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request (GTK_WIDGET (userWindow), 300, 200);
    gtk_window_set_title (GTK_WINDOW (userWindow), "Login");
    g_signal_connect_swapped (userWindow, "delete-event", G_CALLBACK (gtk_widget_destroy), userWindow);

    //vbox
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (userWindow), vbox);
    gtk_widget_show (vbox);

    //usernameLabel
    usernameLabel = gtk_label_new ("Username:");
    gtk_box_pack_start (GTK_BOX (vbox), usernameLabel, TRUE, TRUE, 0);
    gtk_widget_show (usernameLabel);

    //usernameEntry
    usernameEntry = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (usernameEntry), 50);
    g_signal_connect (usernameEntry, "activate", G_CALLBACK (enter_callback), usernameEntry);
    gtk_entry_set_text (GTK_ENTRY (usernameEntry), user);
    tmp_pos = GTK_ENTRY (usernameEntry)->text_length;
    gtk_editable_select_region (GTK_EDITABLE (usernameEntry), 0, GTK_ENTRY (usernameEntry)->text_length);
    gtk_box_pack_start (GTK_BOX (vbox), usernameEntry, TRUE, TRUE, 0);
    gtk_widget_show (usernameEntry);

    //passwordLabel
    passwordLabel = gtk_label_new ("Password:");
    gtk_box_pack_start (GTK_BOX (vbox), passwordLabel, TRUE, TRUE, 0);
    gtk_widget_show (passwordLabel);

    //passwordEntry
    passwordEntry = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (passwordEntry), 50);
    g_signal_connect (passwordEntry, "activate", G_CALLBACK (enter_callback), passwordEntry);
    gtk_entry_set_text (GTK_ENTRY (passwordEntry), password);
    tmp_pos = GTK_ENTRY (passwordEntry)->text_length;
    gtk_editable_select_region (GTK_EDITABLE (passwordEntry), 0, GTK_ENTRY (passwordEntry)->text_length);
    gtk_box_pack_start (GTK_BOX (vbox), passwordEntry, TRUE, TRUE, 0);
    gtk_entry_set_visibility(GTK_ENTRY(passwordEntry), FALSE);
    gtk_widget_show (passwordEntry);

    //button
    button = gtk_button_new_with_label ("Login");
    credentials->password = passwordEntry;
    credentials->username = usernameEntry;
    g_signal_connect_swapped (button, "clicked", G_CALLBACK (addUser), credentials);
    g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), userWindow);
    gtk_widget_set_can_default (button, TRUE);
    gtk_widget_grab_default (button);
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
    gtk_widget_show (button);


    gtk_widget_show (userWindow);

    gtk_main();
}
static void roomModal () {
    GtkWidget *roomWindow;
    GtkWidget *roomEntry;
    GtkWidget *roomLabel;
    GtkWidget *vbox, *hbox;
    gint tmp_pos;
    GtkWidget *button;

    //create a new window
    roomWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request (GTK_WIDGET (roomWindow), 300, 200);
    gtk_window_set_title (GTK_WINDOW (roomWindow), "Login");
    g_signal_connect_swapped (roomWindow, "delete-event", G_CALLBACK (gtk_widget_destroy), roomWindow);

    //vbox
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (roomWindow), vbox);
    gtk_widget_show (vbox);

    //roomLabel
    roomLabel = gtk_label_new ("Enter a room name");
    gtk_box_pack_start (GTK_BOX (vbox), roomLabel, TRUE, TRUE, 0);
    gtk_widget_show (roomLabel);

    //roomEntry
    roomEntry = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (roomEntry), 50);
    g_signal_connect (roomEntry, "activate", G_CALLBACK (enter_callback), roomEntry);
    gtk_entry_set_text (GTK_ENTRY (roomEntry), "");
    tmp_pos = GTK_ENTRY (roomEntry)->text_length;
    gtk_editable_select_region (GTK_EDITABLE (roomEntry), 0, GTK_ENTRY (roomEntry)->text_length);
    gtk_box_pack_start (GTK_BOX (vbox), roomEntry, TRUE, TRUE, 0);
    gtk_widget_show (roomEntry);

    //button
    button = gtk_button_new_with_label ("Create Room");
    g_signal_connect_swapped (button, "clicked", G_CALLBACK (createRoom), roomEntry);
    g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), roomWindow);
    gtk_widget_set_can_default (button, TRUE);
    gtk_widget_grab_default (button);
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
    gtk_widget_show (button);


    gtk_widget_show (roomWindow);
}
static void hostModal() {
    GtkWidget *userWindow;
    GtkWidget *hostEntry;
    GtkWidget *portEntry;
    GtkWidget *hostLabel;
    GtkWidget *portLabel;
    GtkWidget *vbox, *hbox;
    gint tmp_pos;
    GtkWidget *button;
    SOCKET * socket = (SOCKET *) malloc(sizeof(SOCKET*) * 1);

    //create a new window
    userWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request (GTK_WIDGET (userWindow), 300, 200);
    gtk_window_set_title (GTK_WINDOW (userWindow), "Host / Port");
    g_signal_connect_swapped (userWindow, "delete-event", G_CALLBACK (gtk_widget_destroy), userWindow);

    //vbox
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (userWindow), vbox);
    gtk_widget_show (vbox);

    //hostLabel
    hostLabel = gtk_label_new ("Host");
    gtk_box_pack_start (GTK_BOX (vbox), hostLabel, TRUE, TRUE, 0);
    gtk_widget_show (hostLabel);

    //hostEntry
    hostEntry = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (hostEntry), 50);
    gtk_entry_set_text (GTK_ENTRY (hostEntry), host);
    g_signal_connect (hostEntry, "activate", G_CALLBACK (enter_callback), hostEntry);
    tmp_pos = GTK_ENTRY (hostEntry)->text_length;
    gtk_editable_select_region (GTK_EDITABLE (hostEntry), 0, GTK_ENTRY (hostEntry)->text_length);
    gtk_box_pack_start (GTK_BOX (vbox), hostEntry, TRUE, TRUE, 0);
    gtk_widget_show (hostEntry);

    //portLabel
    portLabel = gtk_label_new ("Port");
    gtk_box_pack_start (GTK_BOX (vbox), portLabel, TRUE, TRUE, 0);
    gtk_widget_show (portLabel);

    //portEntry
    portEntry = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (portEntry), 50);
    sprintf(portString, "%d", port);
    gtk_entry_set_text (GTK_ENTRY (portEntry), portString);
    g_signal_connect (portEntry, "activate", G_CALLBACK (enter_callback), portEntry);
    tmp_pos = GTK_ENTRY (portEntry)->text_length;
    gtk_editable_select_region (GTK_EDITABLE (portEntry), 0, GTK_ENTRY (portEntry)->text_length);
    gtk_box_pack_start (GTK_BOX (vbox), portEntry, TRUE, TRUE, 0);
    gtk_widget_show (portEntry);

    //button
    button = gtk_button_new_with_label ("Change");
    socket->port = portEntry;
    socket->host = hostEntry;
    g_signal_connect_swapped (button, "clicked", G_CALLBACK (updateSocket), socket);
    g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), userWindow);
    gtk_widget_set_can_default (button, TRUE);
    gtk_widget_grab_default (button);
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
    gtk_widget_show (button);


    gtk_widget_show (userWindow);

    gtk_main();
}

//-------------------------MAIN--------------------------------------------//
static gboolean commands() {
    //if (gtk_window_is_active(mainWindow)) {
        getMessages();
        update_list_rooms();
        update_list_users();
        sprintf(status, "Username: \"%s\"  Room: \"%s\"\nServer: \"%s:%d\"", user, room, host, port);
        gtk_label_set_text(GTK_LABEL(statusLabel), status);
        bottom = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 10000.0, 0.0, 0.0, 0.0, 0.0));
        gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(messages), bottom);
        return TRUE;
    //} else {return FALSE;}
}
int main( int   argc, char *argv[] ) {
    GtkWidget *list;
    host = (char *) malloc(sizeof(char *) * 50);
    portString = (char *) malloc(sizeof(char *) * 50);
    user = (char *) malloc(sizeof(char *) * 50);
    password = (char *) malloc(sizeof(char *) * 50);
    room = (char *) malloc(sizeof(char *) * 50);
    status = (char *) malloc(sizeof(char *) * 50);
    response = (char*)malloc(MAX_RESPONSE);
    messageResponse = (char*)malloc(MAX_RESPONSE);
    strcpy(host, "localhost");
    strcpy(password, "");
    strcpy(user, "");
    strcpy(room, "");
    sprintf(status, "Username: \"%s\"  Room: \"%s\"\nServer: \"%s:%d\"", user, room, host, port);
    gtk_init (&argc, &argv);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Harrison's IRCClient CS240");
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), window);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_widget_set_size_request (GTK_WIDGET (window), 450, 400);
    mainWindow = GTK_WINDOW(window);

    // Create a table to place the widgets. Use a 7x4 Grid (7 rows x 4 columns)
    GtkWidget *table = gtk_table_new (7, 4, TRUE);
    gtk_container_add (GTK_CONTAINER (window), table);
    gtk_table_set_row_spacings(GTK_TABLE (table), 5);
    gtk_table_set_col_spacings(GTK_TABLE (table), 5);
    gtk_widget_show (table);

    // Add list of users.
    list_users = gtk_list_store_new (1, G_TYPE_STRING);
    update_list_users();
    list = create_list_user("Users", list_users);
    gtk_table_attach_defaults (GTK_TABLE (table), list, 0, 2, 0, 2);
    gtk_widget_show (list);

    // Add list of rooms. Use columns 0 to 4 (exclusive) and rows 0 to 4 (exclusive)
    list_rooms = gtk_list_store_new (1, G_TYPE_STRING);
    list = create_list_room("Rooms", list_rooms);
    update_list_rooms();
    g_signal_connect(roomTreeView, "row-activated", G_CALLBACK(enterRoom), NULL);
    g_signal_connect(roomTreeView, "row-activated", G_CALLBACK(getRoomSelection), NULL);
    gtk_table_attach_defaults (GTK_TABLE (table), list, 2, 4, 0, 2);
    gtk_widget_show (list);

    // Add messages text. Use columns 0 to 4 (exclusive) and rows 4 to 7 (exclusive)
    messages = create_text_receive ("");
    bottom = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 10000.0, 0.0, 0.0, 0.0, 0.0));
    gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(messages), bottom);
    gtk_table_attach_defaults (GTK_TABLE (table), messages, 0, 4, 2, 5);
    gtk_widget_show (messages);
    // Add messages text. Use columns 0 to 4 (exclusive) and rows 4 to 7 (exclusive)

    //labels for current room and user
    statusLabel = gtk_label_new (status);
    gtk_table_attach_defaults(GTK_TABLE (table), statusLabel, 0, 4, 5, 6);
    //gtk_label_set_justify (GTK_LABEL(statusLabel), GTK_JUSTIFY_LEFT);
    gtk_widget_show (statusLabel);

    myMessage = create_text_send ("");
    gtk_table_attach_defaults (GTK_TABLE (table), myMessage, 0, 4, 6, 7);
    gtk_widget_show (myMessage);

    // Add send button. Use columns 0 to 1 (exclusive) and rows 4 to 7 (exclusive)
    GtkWidget *send_button = gtk_button_new_with_label ("Send");
    gtk_table_attach_defaults(GTK_TABLE (table), send_button, 0, 1, 7, 8);
    gtk_widget_show (send_button);
    g_signal_connect (send_button, "clicked", G_CALLBACK (sendMessage), NULL);

    // Add room button
    GtkWidget *createRoom_button = gtk_button_new_with_label ("Create\nRoom");
    gtk_table_attach_defaults(GTK_TABLE (table), createRoom_button, 2, 3, 7, 8);
    gtk_widget_show (createRoom_button);
    g_signal_connect (createRoom_button, "clicked", G_CALLBACK (roomModal), NULL);

    // Add login button
    GtkWidget *login_button = gtk_button_new_with_label ("Login");
    gtk_table_attach_defaults(GTK_TABLE (table), login_button, 3, 4, 7, 8);
    gtk_widget_show (login_button);
    g_signal_connect (login_button, "clicked", G_CALLBACK (loginModal), NULL);

    // Add socket button
    GtkWidget *socket_button = gtk_button_new_with_label ("Host\nPort");
    gtk_table_attach_defaults(GTK_TABLE (table), socket_button, 1, 2, 7, 8);
    gtk_widget_show (socket_button);
    g_signal_connect (socket_button, "clicked", G_CALLBACK (hostModal), NULL);

    gtk_widget_show (table);
    gtk_widget_show (window);

    g_timeout_add(2000, (GSourceFunc) commands, (gpointer) window);

    gtk_main ();

    return 0;
}

