## zbus-cli-ent

This project provides a CLI for sending events to [zBus][] and an interactive text-based UI for
sending events to, and receiving events from, [zBus][] (similar to [zbus-util][]).

The UI portion of the project was built using [ncurses][].

### Build & Run

This project can be built and run on a Linux machine, or inside a [Docker][] container.

Building and running locally requires some version of [Qt][] 5. With [Qt][] installed, navigate to
the project root, build with `qmake && make`, then run with `./zbus-cli-ent.x`.

Building and running inside a [Docker][] container can be done by navigating to the project root,
building with `docker-compose run build`, then running with `docker-compose run client`.

### Commands & Arguments

`zbus-cli-ent.x` takes two arguments:
- `-w, --websocket <url>`: **REQUIRED** Takes the URL to the websocket that the zBus server is
                           listening to. **NOTE**: If `zbus-cli-ent.x` is run inside a [Docker][]
                           container, the machine's IP address will need to be used in place of
                           `localhost`.

- `-s, --send <event>`: Takes a JSON-formatted zBus event to be sent to the zBus server. If this
                        argument is not provided, `zbus-cli-ent.x` will start the interactive
                        text-based UI.

There are four [Docker][] commands:
- `docker-compose run build` builds the `zbus-cli-ent.x` application.

- `docker-compose run client` runs the `zbus-cli-ent.x` application. Arguments provided to this
                              command are received by the application.

- `docker-compose run shell` starts a bash shell inside the docker container with project repository
                             mounted inside of it.

- `docker-compose run clean` removes all build artifacts.

- `docker-compose run check` runs the unit tests.

Additionally, there is a simple bash script to check the connection to zBus:
- `./zbus-curl-test.sh <url>` negotiates a websocket connection with the zBus server at the given
                              URL.

### Examples

Send a single event to zBus (note the use of single quotes for the JSON string):
```bash
docker-compose run client \
    --websocket ws://10.0.0.42:8180 \
    --send '{"event":"sender.type","data":"data"}'
```

Send multiple events to zBus:
```bash
docker-compose run client \
    --websocket ws://10.0.0.42:8180 \
    --send '{"event":"sender.type","data":"data"}'
    --send '{"event":"rednes.epyt","data":{"key":"value"}}'
```

Start the interactive text-based UI:
```bash
docker-compose run client \
    --websocket ws://10.0.0.42:8180
```

Test the connection to zBus (note the exclusion of the `ws://` protocol from the URL):
```bash
./zbus-curl-test.sh 10.0.0.42:8180
```

### Mocking the Pinpad

**DEPRECATED**: Mocking the pinpad can now be automated by toggling on the pinpad simulator in
command mode with `m`. With an SSH tunnel to [zBus][] and [Gemini][] , the pinpad simulator will
emit the events listed below when it detects that POS has started the payment flow.

To convince a local instance of [POS][] that it is connected to a pinpad, open an SSH tunnel for
[zBus][] and [Gemini][] to a physical store box with pinpads connected. In [POS][], select a register
number that corresponds to a physical register (or thin client) that has a pinpad. To clear the
"No Pinpad" error message in [POS][], send a `pinpad.displayItemSuccess` message from
`zbus-cli-ent`.

At this point, the "Credit/Debit" button should be enabled. After beginning the payment
authorization flow from [POS][], use the mock menu in `zbus-cli-ent` to send the following events in
order: 
1. `pinpad.cardInserted`
2. `pinpad.cardRead`
3. `pinpad.paymentAccepted`
4. `pinpad.cardRemoved`
5. `pinpad.finishTransaction`

`zbus-cli-ent` will handle populating each event with the appropriate data, `requestId`, and
`authAttemptId`.

[Docker]: https://docs.docker.com/get-docker/
[Gemini]: https://gitlab.autozone.com/store-operations/azstore/-/tree/master/gemini
[POS]: https://gitlab.autozone.com/store-operations/pos-rearch/azstore/-/tree/master/web/packages/pos-webui
[ncurses]: https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/intro.html
[Qt]: https://www.qt.io/download
[zBus]: https://gitlab.autozone.com/store-operations/azstore/-/tree/master/zbus
[zbus-util]: https://gitlab.autozone.com/10707207/zbus-util
