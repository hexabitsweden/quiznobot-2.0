# quiznobot
Automatically exported from code.google.com/p/quiznobot

Quiznobot - A simple IRC XDCC bot to facilitate the sharing of files via the various IRC networks.

This is a simple IRC XDCC bot written in C to facilitate the distribution of files on the various IRC Networks. 
It is designed with Unix server administrators in mind so it will be easy for them to add it to their init scripts to provide files they specify to the world. 
We need testersI can confirm that the bot will send files to a single client over the internet, but I cannot confirm that it will work with multiple clients. 

Big thanks to the original developer quizno50@gmail.com!

Hexabit rescued this project from Google Code.

/////////////////////////////////////////////////////////////////////////////
//  Instructions:
//    The bot is very simple to use:
//      quiznoBot -n [nick] -c [channel] -s [server] -p [port] -d [dir] (-v)(-v)
//    The options work as follows:
//      -n [nick] -- Specify the name of the bot on the IRC network.
//      -c [channel] -- specify the channel the bot will join.
//      -s [server] -- specify the server the bot will join (IPV4 only please)
//      -p [port] -- specify the port the server will use
//      -d [dir] -- specify the directory the server will share.
//      -v -- increase the debug level (amount of information printed to the
//            user, one should be plenty for admins, two is good for developers
//      -e [ip] -- Sets the external IP in case the bot is behind a firewall
//                 or NAT and cannot receive incomming connections on its 
//                 network adapter's IP address.
/////////////////////////////////////////////////////////////////////////////
