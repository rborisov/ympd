/* ympd
   (c) 2013-2014 Andrew Karpow <andy@ndyk.de>
   This project's homepage is: http://www.ympd.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

var socket;
var last_state;
var current_app;
var pagination = 0;
//var queue_pagination = 0;
var radio_pagination = 0;
var browsepath;
var lastSongTitle = "";
var current_song = new Object();
var MAX_ELEMENTS_PER_PAGE = 5;
var current_song_pos = 0;
var next_song_pos = 0;

var app = $.sammy(function() {
    function prepare() {
        $('#nav_links > li').removeClass('active');
        $('.page-btn').addClass('disabled');//.addClass('hide');
        browsepath = '';
    }

    this.get(/\#\/(\d+)/, function() {
        prepare();
//        queue_pagination = 0;
//        queue_pagination = parseInt(this.params['splat'][0]);
    });

    this.get("/", function(context) {
        context.redirect("#/0");
    });
/*    this.get("/menu.html", function(context) {
        context.redirect("/menu.html");
    });*/

});

$(document).ready(function(){
    webSocketConnect();
});


function webSocketConnect() {
    if (typeof MozWebSocket != "undefined") {
        socket = new MozWebSocket(get_appropriate_ws_url());
    } else {
        socket = new WebSocket(get_appropriate_ws_url());
    }

    try {
        socket.onopen = function() {
            console.log("connected");
            updateNotificationIcon(0);

            app.run();
        }

        socket.onmessage = function got_packet(msg) {
        if(msg.data === last_state || msg.data.length == 0)
            return;

        var obj = JSON.parse(msg.data);
        switch (obj.type) {
                case "state":
                    updatePlayIcon(obj.data.state);
                    $('#volume').text(obj.data.volume);
                    updateVolumeIcon(obj.data.volume);
                    updateRadioIcon(obj.data.radio_status);
                    updateRandomIcon(obj.data.random);
                    updateRepeatIcon(obj.data.repeat);

                    if(JSON.stringify(obj) === JSON.stringify(last_state))
                        break;

                    current_song.totalTime  = obj.data.totalTime;
                    current_song.currentSongId = obj.data.currentsongid;
                    current_song_pos = obj.data.songpos;
                    $('#songrating').text(obj.data.songrating);
                    $('#currentpos').text(current_song_pos+"("+obj.data.queue_len+")");
                    next_song_pos = obj.data.nextsongpos;
                    $('#nextpos').text(next_song_pos);
                    var total_minutes = Math.floor(obj.data.totalTime / 60);
                    var total_seconds = obj.data.totalTime - total_minutes * 60;

                    var elapsed_minutes = Math.floor(obj.data.elapsedTime / 60);
                    var elapsed_seconds = obj.data.elapsedTime - elapsed_minutes * 60;

                    $('#counter')
                    .text(elapsed_minutes + ":" +
                        (elapsed_seconds < 10 ? '0' : '') + elapsed_seconds + "/" +
                        total_minutes + ":" + (total_seconds < 10 ? '0' : '') + total_seconds);

                    last_state = obj;
                    break;
                case "disconnected":
                    updateNotificationIcon(2);
                    break;
/*                case "update_queue":
                    if(current_app === 'queue') {
                        socket.send('MPD_API_GET_QUEUE,'+queue_pagination);
                    }
                    break;*/
                case "current_radio":
                    $('#currentradio').text(obj.data.name);
                    $('#currentradiostatus').text(obj.data.status);
                    $('#currentradiosize').text(obj.data.size);
                    break;
                case "artist_info":
                    if (obj.data.artist && obj.data.art) {
                        var artimage = document.getElementById("artimage");
                        artimage.src = obj.data.art;
                    } else {
                        if (obj.data.artist)
                            download_artist_info(obj.data.artist);
                    }
                    break;
                case "track_info":
                    if (obj.data.title && obj.data.artist) {
                        var artimage = document.getElementById("artimage");
                        artimage.src = "art.png";
                        $('#album').text("");
                        $.get("http://ws.audioscrobbler.com/2.0/?method=track.getinfo&artist=" +
                                obj.data.artist + "&track=" + obj.data.title +
                                "&autocorrect=1&api_key=ecb4076a85c81aae38a7e8f11e42a0b1&format=json&callback=",
                                function(lastfm)
                                {
                                    var art_url;
                                    //var artimage = document.getElementById("artimage");
                                    if (lastfm && lastfm.track && lastfm.track.album) {
                                        if (lastfm.track.album.image &&
                                            lastfm.track.album.image[1]['#text'].indexOf("default_album") == -1) {
                                            art_url = lastfm.track.album.image[1]['#text'];
                                            console.log(art_url);
                                            artimage.src = art_url;
                                        } else {
                                            //artimage.src = "/images/art.jpg";
                                            console.log("there is no track.album.image");
                                            //socket.send('MPD_API_DB_GET_ARTIST,'+obj.data.artist);
                                        }
                                        if (lastfm.track.album.title) {
                                            $('#album').text(lastfm.track.album.title);
                                        } else {
                                            //$('#album').text("");
                                        }
                                        if (lastfm.track.album) {
                                            socket.send('MPD_API_DB_ALBUM,'+obj.data.title+'|'+obj.data.artist+'|'+
                                                    lastfm.track.album.title+'|'+art_url);
                                        }
                                    } else {
                                        //artimage.src = "/images/art.jpg";
                                        console.log("there is no track info "+obj.data.artist+" "+obj.data.title);
                                        socket.send('MPD_API_DB_GET_ARTIST,'+obj.data.artist);
                                    }
                                })
                                //.done (function ()   { console.log("done"  ); })
                                .fail (function ()   { console.log("fail"  ); })
                                .error (function()   {
                                    console.log("error" );
                                    socket.send('MPD_API_DB_GET_ARTIST,'+obj.data.artist);
                                })
                                //.always (function()  { console.log("always"); });
                    }
                    break;
                case "song_change":
                    $('#currenttrack').text(" " + obj.data.title);

                    if(obj.data.album) {
                        $('#album').text(obj.data.album);
                    } else
                       $('#album').text("");

                    if(obj.data.artist) {
                        $('#artist').text(obj.data.artist);
                    } else
                        $('#artist').text("");

                    if(obj.data.art && obj.data.art.indexOf("default_album") == -1) {
                        console.log(obj.data.art);
                        var art_url = obj.data.art;
                        document.getElementById("artimage").src = art_url;
                    } else {
                        console.log("no art" );
                        if(obj.data.artist)
                            socket.send('MPD_API_DB_GET_ARTIST,'+obj.data.artist);
                    }

//                    socket.send('MPD_API_GET_QUEUE,'+queue_pagination);

                    break;
                case "mpdhost":
                    $('#mpdhost').val(obj.data.host);
                    $('#mpdport').val(obj.data.port);
                    if(obj.data.passwort_set)
                        $('#mpd_password_set').removeClass('hide');
                    break;
                case "error":
                    console.log(obj.data);
                    updateNotificationIcon(1);
                default:
                    break;
            }


        }
        socket.onclose = function(){
            console.log("disconnected");
            updateNotificationIcon(2);

            webSocketConnect();
        }

    } catch(exception) {
        alert('<p>Error' + exception);
    }

}

function get_appropriate_ws_url()
{
    var pcol;
    var u = document.URL;

    /*
    /* We open the websocket encrypted if this page came on an
    /* https:// url itself, otherwise unencrypted
    /*/

    if (u.substring(0, 5) == "https") {
        pcol = "wss://";
        u = u.substr(8);
    } else {
        pcol = "ws://";
        if (u.substring(0, 4) == "http")
            u = u.substr(7);
    }

    u = u.split('/');

    return pcol + u[0];
}

var download_artist_info = function(artist)
{
    $.get("http://ws.audioscrobbler.com/2.0/?method=artist.getinfo&artist=" +
        artist +
        "&autocorrect=1&api_key=ecb4076a85c81aae38a7e8f11e42a0b1&format=json&callback=",
    function(lastfm)
    {
        var art_url;
        var artimage = document.getElementById("artimage");
        if (lastfm && lastfm.artist) {
            if (lastfm.artist.image[2]['#text']) {
                art_url = lastfm.artist.image[1]['#text'];
                console.log('art_url :'+art_url);
                artimage.src = art_url;
                socket.send('MPD_API_DB_ARTIST,'+artist+'|'+art_url);
            } else {
                console.log("there is no artist image");
            }
        } else {
            console.log("there is no artist info");
        }
    });
}

var updateNotificationIcon = function(notify)
{
    $("#notification-icon").removeClass("glyphicon-ok")
    .removeClass("glyphicon-flag")
    .removeClass("glyphicon-remove");

    if (notify == 0) { //connected
        $("#notification-icon").addClass("glyphicon-ok");
    } else if (notify == 1) { //error
        $("#notification-icon").addClass("glyphicon-flag");
    } else { //disconnected
        $("#notification-icon").addClass("glyphicon-remove");
    }
}

var updateVolumeIcon = function(volume)
{
    $("#volume-icon").removeClass("glyphicon-volume-off");
    $("#volume-icon").removeClass("glyphicon-volume-up");
    $("#volume-icon").removeClass("glyphicon-volume-down");

    if(volume == 0) {
        $("#volume-icon").addClass("glyphicon-volume-off");
    } else if (volume < 50) {
        $("#volume-icon").addClass("glyphicon-volume-down");
    } else {
        $("#volume-icon").addClass("glyphicon-volume-up");
    }
}

var updatePlayIcon = function(state)
{
    $("#play-icon").removeClass("glyphicon-play")
    .removeClass("glyphicon-pause");
    $('#track-icon').removeClass("glyphicon-play")
    .removeClass("glyphicon-pause")
    .removeClass("glyphicon-stop");

    if(state == 1) { // stop
        $("#play-icon").addClass("glyphicon-play");
        $('#track-icon').addClass("glyphicon-stop");
    } else if(state == 2) { // pause
        $("#play-icon").addClass("glyphicon-pause");
        $('#track-icon').addClass("glyphicon-play");
    } else { // play
        $("#play-icon").addClass("glyphicon-play");
        $('#track-icon').addClass("glyphicon-pause");
    }
}

var updateRadioIcon = function(radio_status)
{
    $("#cloud-icon").removeClass("glyphicon-cloud-download")
    .removeClass("glyphicon-cloud");
    if (radio_status == 1) { //on
        $("#cloud-icon").addClass("glyphicon-cloud-download");
    } else
    {
        $("#cloud-icon").addClass("glyphicon-cloud");
    }
}

var updateRandomIcon = function(random)
{
    $("#random-icon").removeClass('glyphicon-random')
    .removeClass("glyphicon-sort-by-order");
    if (random == 0) {
        $("#random-icon").addClass('glyphicon-sort-by-order');
    } else {
        $("#random-icon").addClass('glyphicon-random');
    }
}

var updateRepeatIcon = function(repeat)
{
    $("#repeat-icon").removeClass('glyphicon-repeat')
    .removeClass("glyphicon-play-circle");
    if (repeat == 0) {
        $("#repeat-icon").addClass('glyphicon-play-circle');
    } else {
        $("#repeat-icon").addClass('glyphicon-repeat');
    }
}

function updateDB() {
    socket.send('MPD_API_UPDATE_DB');
}

function clickPlay() {
    if($('#track-icon').hasClass('glyphicon-stop'))
        socket.send('MPD_API_SET_PLAY');
    else
        socket.send('MPD_API_SET_PAUSE');
}

function clickRandom() {
    socket.send("MPD_API_TOGGLE_RANDOM," + ($("#random-icon").hasClass('gray') ? 1 : 0));
}

function basename(path) {
    return path.split('/').reverse()[0];
}
