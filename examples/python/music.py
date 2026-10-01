"""Cadence: a native, offline music-player mock. No audio or network access."""
import os
from contextlib import ExitStack
from pathlib import Path
import random
from dataclasses import dataclass
import cui


@dataclass(frozen=True)
class Track:
    title: str
    artist: str
    album: str
    seconds: int


TRACKS = (
    Track('Soft Focus', 'The Paper Planes', 'Slow mornings', 218),
    Track('Golden Hour', 'June & The Sea', 'Daylight', 194),
    Track('Somewhere Quiet', 'Northbound', 'Open windows', 246),
    Track('Blue Afternoon', 'Milo Park', 'Daylight', 203),
    Track('A Little Longer', 'The Paper Planes', 'Slow mornings', 231),
    Track('Home Again', 'June & The Sea', 'Daylight', 187),
    Track('Night Windows', 'Milo Park', 'After hours', 224),
    Track('Quiet Streets', 'Northbound', 'After hours', 257),
    Track('Sunday Coffee', 'Lena Moss', 'Slow mornings', 192),
    Track('First Light', 'Lena Moss', 'Daylight', 211),
    Track('Last Train', 'The Paper Planes', 'After hours', 243),
    Track('Coastline', 'Northbound', 'Open windows', 269),
)


def duration(seconds):
    return f'{int(seconds) // 60}:{int(seconds) % 60:02}'


def artwork():
    """An original, procedural cover. RGBA pixels, no asset dependencies."""
    pixels = bytearray()
    for row in range(640):
        y = row / 4
        for column in range(640):
            x = column / 4
            sun = (x - 100)**2 + (y - 44)**2 < 25**2
            ridge = y > 110 + 18 * ((x / 50) % 2 - 1)**2
            color = (247, 203, 137) if sun else (36, 72, 67) if ridge else (80 + y//3, 118 + y//4, 119 + x//6)
            pixels.extend((*map(int, color), 255))
    return bytes(pixels)


class Cadence:
    def __init__(self, app, icons):
        self.app = app
        self.icons = icons
        self.current = 0
        self.playing = False
        self.position = 0.0
        self.likes = {1, 3}
        self.collection = 'All tracks'
        self.visible = []
        self.playlists = {'Morning ritual': [0, 1, 8], 'After the studio': [6, 7, 10]}
        self.queue = [1, 6, 11]
        self.history = []
        self.random = random.Random()
        self.last_volume = .7
        self.last_tick = cui.monotonic_time()
        self.window = app.window('Cadence - Python music player', 1480, 1000)
        self.window.scrollable()
        root = self.window.root
        root.padding(28)
        header = root.box(cui.HORIZONTAL, 14)
        header.label('Cadence').role(cui.TITLE)
        header.label('Your listening room').role(cui.CAPTION)
        self.build_library(root)
        self.build_player(root)
        self.status = root.label('Your local listening room · no audio or account connection')
        self.status.role(cui.CAPTION)
        self.refresh_library()
        self.render_player()
        app.every(200, self.tick)
        self.window.show()

    def build_library(self, root):
        body = root.box(cui.HORIZONTAL, 28)
        body.expand()
        side = body.box(gap=14)
        side.min_size(175, 0)
        side.label('YOUR LIBRARY').role(cui.CAPTION)
        self.all_button = side.button('All tracks').on_action(lambda _: self.browse('All tracks'))
        self.all_button.set_icon(cui.ICON_MENU); self.all_button.role(cui.FLAT)
        self.liked_button = side.button('Liked songs').on_action(lambda _: self.browse('Liked songs'))
        self.liked_button.set_icon(cui.ICON_HEART); self.liked_button.role(cui.FLAT)
        side.separator()
        side.label('COLLECTIONS').role(cui.CAPTION)
        side.button('Slow mornings').on_action(lambda _: self.browse('Slow mornings'))
        side.button('Daylight').on_action(lambda _: self.browse('Daylight'))
        side.button('After hours').on_action(lambda _: self.browse('After hours'))
        side.separator()
        side.label('YOUR PLAYLISTS').role(cui.CAPTION)
        self.playlist_list = side.list(list(self.playlists))
        self.playlist_list.min_size(190, 155)
        self.playlist_list.on_action(lambda _: self.open_playlist())
        self.playlist_name = side.entry('')
        self.playlist_name.placeholder('Name a new playlist')
        self.create_playlist_button = side.button('+ Create playlist').on_action(lambda _: self.create_playlist())
        main = body.box(gap=16)
        main.expand()
        hero = main.box(cui.HORIZONTAL, 24)
        with cui.Icon.rgba(artwork(), 640, 640) as art:
            cover = hero.icon(art)
            cover.icon_size(200)
        copy = hero.box(gap=10)
        copy.label('THE DAILY MIX').role(cui.CAPTION)
        copy.label('Find your\nquieter rhythm.').role(cui.TITLE)
        copy.label('A collection for slow mornings\nand late nights in the studio.')
        copy.label('12 tracks · Curated for you').role(cui.CAPTION)
        self.search = main.search('')
        self.search.placeholder('Search songs, artists or albums')
        self.search.on_action(lambda _: self.refresh_library())
        self.collection_label = main.label('')
        self.collection_label.role(cui.HEADING)
        tools = main.box(cui.HORIZONTAL, 8)
        self.enqueue_button = tools.button('+ Add to queue').on_action(lambda _: self.enqueue())
        self.playlist_target = tools.select(list(self.playlists))
        self.playlist_target.selected = 0
        self.add_playlist_button = tools.button('Add to playlist').on_action(lambda _: self.add_to_playlist())
        self.remove_playlist_button = tools.button('Remove').on_action(lambda _: self.remove_from_playlist())
        self.tracks = main.table(['Title', 'Artist', 'Album', 'Time'])
        self.tracks.expand()
        self.tracks.on_action(lambda _: self.select_track())
        self.empty = main.label('No tracks found. Try another search or collection.')
        self.build_queue(body)

    def build_queue(self, body):
        panel = body.box(gap=12)
        panel.min_size(265, 0)
        panel.label('UP NEXT').role(cui.CAPTION)
        self.queue_heading = panel.label('Your listening queue')
        self.queue_heading.role(cui.HEADING)
        self.queue_list = panel.list([])
        self.queue_list.expand()
        self.queue_list.on_action(lambda _: self.queue_controls())
        order = panel.box(cui.HORIZONTAL, 8)
        self.queue_up = order.icon_button(cui.ICON_UP, 'Move up').on_action(lambda _: self.move_queue(-1))
        self.queue_down = order.icon_button(cui.ICON_DOWN, 'Move down').on_action(lambda _: self.move_queue(1))
        self.queue_play = panel.button('Play selected now').on_action(lambda _: self.play_queued())
        self.queue_remove = panel.button('Remove selected').on_action(lambda _: self.remove_queued())
        self.queue_clear = panel.button('Clear queue').on_action(lambda _: self.clear_queue())
        panel.separator()
        panel.label('RECENTLY PLAYED').role(cui.CAPTION)
        self.recent = panel.label('Your session starts here.')
        self.recent.role(cui.CAPTION)
        self.refresh_queue()


    def build_player(self, root):
        root.separator()
        player = root.box(cui.HORIZONTAL, 20)
        info = player.box(gap=4)
        info.min_size(240, 0)
        self.now_title = info.label('')
        self.now_title.role(cui.HEADING)
        self.now_artist = info.label('')
        self.now_artist.role(cui.CAPTION)
        controls = player.box(gap=8)
        controls.expand()
        buttons = controls.box(cui.HORIZONTAL, 12)
        buttons.label('').expand()
        self.previous = buttons.icon_button(cui.ICON_PREVIOUS, 'Previous').on_action(lambda _: self.advance(-1))
        self.play = buttons.icon_button(self.icons['play'], 'Play').on_action(lambda _: self.toggle_play())
        self.play.role(cui.PRIMARY)
        self.play.icon_size(28)
        self.next = buttons.icon_button(cui.ICON_NEXT, 'Next').on_action(lambda _: self.advance(1))
        self.like = buttons.icon_button(cui.ICON_HEART, 'Like').on_action(lambda _: self.toggle_like())
        self.repeat = buttons.toggle('Repeat track', False)
        self.repeat.set_icon(cui.ICON_REPEAT); self.repeat.icon_only(); self.repeat.role(cui.FLAT)
        self.shuffle = buttons.toggle('Shuffle', False)
        self.shuffle.set_icon(cui.ICON_SHUFFLE); self.shuffle.icon_only(); self.shuffle.role(cui.FLAT)
        buttons.label('').expand()
        timeline = controls.box(cui.HORIZONTAL, 10)
        self.seek = timeline.slider(0)
        self.seek.expand()
        self.seek.on_action(lambda _: self.scrub())
        self.clock = timeline.label('')
        volume = player.box(gap=6)
        self.volume_label = volume.label('Volume · 70%')
        self.volume = volume.slider(.7)
        self.volume.min_size(130, 0)
        self.volume.on_action(lambda _: self.volume_changed())
        self.mute_button = volume.icon_button(cui.ICON_VOLUME, 'Mute').on_action(lambda _: self.toggle_mute())
        root.label('OFFLINE DEMO · Playback is simulated. No audio, streaming or account connection.').role(cui.CAPTION)

    def browse(self, collection):
        self.collection = collection
        self.refresh_library()

    def refresh_library(self):
        query = self.search.text.strip().casefold()
        self.visible = [i for i, t in enumerate(TRACKS)
                        if (self.collection == 'All tracks' or t.album == self.collection
                            or self.collection == 'Liked songs' and i in self.likes
                            or i in self.playlists.get(self.collection, []))
                        and query in f'{t.title} {t.artist} {t.album}'.casefold()]
        self.tracks.rows([[TRACKS[i].title, TRACKS[i].artist, TRACKS[i].album, duration(TRACKS[i].seconds)] for i in self.visible])
        self.tracks.selected = self.visible.index(self.current) if self.current in self.visible else -1
        self.empty.visible(not self.visible)
        total = sum(TRACKS[i].seconds for i in self.visible)
        self.collection_label.text = f'{self.collection}  ·  {len(self.visible)} songs · {total // 60} min'
        self.remove_playlist_button.enabled(self.collection in self.playlists and self.current in self.playlists.get(self.collection, []))

    def select_track(self):
        row = self.tracks.selected
        if row < 0:
            return
        source = self.tracks.table_source_row(row)
        if 0 <= source < len(self.visible):
            if self.current != self.visible[source]:
                self.history.append(self.current)
                self.history = self.history[-12:]
            self.current = self.visible[source]
            self.position = 0
            self.last_tick = cui.monotonic_time()
            self.render_player()
            self.refresh_library()

    def render_player(self):
        track = TRACKS[self.current]
        self.now_title.text = track.title
        self.now_artist.text = track.artist
        self.play.text = 'Pause' if self.playing else 'Play'
        self.play.set_icon(self.icons['pause' if self.playing else 'play'])
        self.like.text = 'Unlike' if self.current in self.likes else 'Like'
        self.like.set_icon(cui.ICON_HEART_FILLED if self.current in self.likes else cui.ICON_HEART)
        self.seek.value = self.position / track.seconds
        self.clock.text = f'{duration(self.position)} / {duration(track.seconds)}'
        names = [TRACKS[i].title for i in self.history[-4:]][::-1]
        self.recent.text = '\n'.join(names) or 'Your session starts here.'

    def toggle_play(self):
        self.playing = not self.playing
        self.last_tick = cui.monotonic_time()
        self.render_player()

    def toggle_like(self):
        if self.current in self.likes:
            self.likes.remove(self.current)
        else:
            self.likes.add(self.current)
        self.refresh_library()
        self.render_player()

    def advance(self, direction):
        if direction < 0 and self.history:
            target = self.history.pop()
        else:
            target = self.next_track(direction)
            self.history = (self.history + [self.current])[-12:]
        self.current = target
        self.position = 0
        self.last_tick = cui.monotonic_time()
        self.refresh_library()
        self.refresh_queue()
        self.render_player()

    def next_track(self, direction):
        if direction > 0 and self.queue:
            return self.queue.pop(0)
        collection = self.visible or list(range(len(TRACKS)))
        choices = [i for i in collection if i != self.current]
        if direction > 0 and self.shuffle.checked and choices:
            return self.random.choice(choices)
        index = collection.index(self.current) if self.current in collection else (-1 if direction > 0 else 0)
        return collection[(index + direction) % len(collection)]

    def refresh_queue(self, selected=-1):
        self.queue_list.items([f'{n+1:02}  {TRACKS[i].title}\n{TRACKS[i].artist}' for n, i in enumerate(self.queue)])
        self.queue_list.selected = min(selected, len(self.queue)-1)
        self.queue_heading.text = f'{len(self.queue)} tracks · {sum(TRACKS[i].seconds for i in self.queue)//60} min'
        self.queue_controls()

    def queue_controls(self):
        row = self.queue_list.selected
        valid = 0 <= row < len(self.queue)
        self.queue_up.enabled(valid and row > 0)
        self.queue_down.enabled(valid and row+1 < len(self.queue))
        self.queue_play.enabled(valid)
        self.queue_remove.enabled(valid)
        self.queue_clear.enabled(bool(self.queue))

    def enqueue(self):
        if len(self.queue) >= 32:
            self.status.text = 'The queue is full (32 tracks). Remove a track first.'
            return
        self.queue.append(self.current)
        self.refresh_queue(len(self.queue)-1)
        self.status.text = f'Queued {TRACKS[self.current].title}'

    def move_queue(self, direction):
        row = self.queue_list.selected
        target = row + direction
        if 0 <= row < len(self.queue) and 0 <= target < len(self.queue):
            self.queue[row], self.queue[target] = self.queue[target], self.queue[row]
            self.refresh_queue(target)

    def play_queued(self):
        row = self.queue_list.selected
        if not 0 <= row < len(self.queue):
            return
        self.history = (self.history + [self.current])[-12:]
        self.current = self.queue.pop(row)
        self.position = 0
        self.playing = True
        self.last_tick = cui.monotonic_time()
        self.refresh_queue(row)
        self.refresh_library()
        self.render_player()

    def remove_queued(self):
        row = self.queue_list.selected
        if 0 <= row < len(self.queue):
            self.queue.pop(row)
            self.refresh_queue(row)

    def clear_queue(self):
        self.queue.clear()
        self.refresh_queue()

    def open_playlist(self):
        names = list(self.playlists)
        row = self.playlist_list.selected
        if 0 <= row < len(names):
            self.browse(names[row])
            self.playlist_target.selected = row

    def create_playlist(self):
        name = self.playlist_name.text.strip()
        reserved = ['All tracks', 'Liked songs', *[t.album for t in TRACKS], *self.playlists]
        if not name or len(name) > 32 or len(self.playlists) >= 8 or name.casefold() in [n.casefold() for n in reserved]:
            self.status.text = 'Use a unique playlist name, 1–32 characters. Limit: 8 playlists.'
            return
        self.playlists[name] = []
        self.playlist_list.items(list(self.playlists))
        self.playlist_target.items(list(self.playlists))
        self.playlist_target.selected = len(self.playlists)-1
        self.playlist_list.selected = len(self.playlists)-1
        self.playlist_name.text = ''
        self.browse(name)
        self.status.text = 'Playlist created. Select a library track and add it here.'

    def add_to_playlist(self):
        names = list(self.playlists)
        row = self.playlist_target.selected
        if not 0 <= row < len(names):
            return
        name = names[row]
        if self.current not in self.playlists[name]:
            self.playlists[name].append(self.current)
        self.refresh_library()
        self.status.text = f'{TRACKS[self.current].title} is in {name}'

    def remove_from_playlist(self):
        playlist = self.playlists.get(self.collection, [])
        if self.current in playlist:
            playlist.remove(self.current)
            self.refresh_library()
            self.status.text = 'Removed from playlist; the track stays in your library.'

    def volume_changed(self):
        self.volume_label.text = f'Volume · {self.volume.value:.0%}'
        self.mute_button.set_icon(cui.ICON_MUTED if self.volume.value == 0 else cui.ICON_VOLUME)
        self.mute_button.text = 'Unmute' if self.volume.value == 0 else 'Mute'

    def toggle_mute(self):
        if self.volume.value > 0:
            self.last_volume = self.volume.value
            self.volume.value = 0
        else:
            self.volume.value = self.last_volume
        self.volume_changed()

    def scrub(self):
        self.position = self.seek.value * TRACKS[self.current].seconds
        self.last_tick = cui.monotonic_time()
        self.render_player()

    def step(self, elapsed):
        if not self.playing:
            return
        self.position += elapsed
        if self.position >= TRACKS[self.current].seconds:
            if self.repeat.checked:
                self.position %= TRACKS[self.current].seconds
            else:
                self.advance(1)
        self.render_player()

    def tick(self):
        now = cui.monotonic_time()
        elapsed = now - self.last_tick
        self.last_tick = now
        self.step(elapsed)

    def smoke(self):
        self.search.text = 'Somewhere Quiet'
        self.refresh_library()
        assert self.visible == [2]
        self.tracks.selected = 0
        self.select_track()
        self.play.activate(); self.step(3)
        assert self.current == 2 and self.position == 3
        self.play.activate(); self.step(2)
        assert self.position == 3
        self.seek.value = .5; self.scrub()
        assert self.position == TRACKS[2].seconds / 2
        self.like.activate(); assert 2 in self.likes
        self.search.text = ''
        self.liked_button.activate(); assert self.visible == [1, 2, 3]
        self.like.activate(); assert self.visible == [1, 3]
        self.smoke_queue()
        self.smoke_playlists()
        self.mute_button.activate(); assert self.volume.value == 0
        self.mute_button.activate(); assert abs(self.volume.value-.7) < .001
        self.clear_queue(); self.repeat.checked = True
        self.playing = True; current = self.current
        self.position = TRACKS[current].seconds-1; self.step(2)
        assert self.current == current and self.position == 1
        self.repeat.checked = False; self.shuffle.checked = True
        self.browse('All tracks'); self.next.activate()
        assert self.current != current
        self.search.text = 'nothing matches'; self.refresh_library()
        assert not self.visible
        assert not self.app.error
        print('Cadence advanced smoke passed', flush=True)
        self.app.quit()

    def smoke_queue(self):
        self.queue_clear.activate(); assert not self.queue
        self.enqueue_button.activate(); assert self.queue == [2]
        self.current = 3; self.enqueue_button.activate(); assert self.queue == [2, 3]
        self.queue_up.activate(); assert self.queue == [3, 2]
        self.queue_down.activate(); assert self.queue == [2, 3]
        self.queue_play.activate(); assert self.current == 3 and self.queue == [2]
        self.next.activate(); assert self.current == 2 and not self.queue
        self.previous.activate(); assert self.current == 3
        self.enqueue_button.activate(); self.queue_remove.activate(); assert not self.queue

    def smoke_playlists(self):
        self.playlist_name.text = 'Road trip'; self.create_playlist_button.activate()
        assert 'Road trip' in self.playlists and not self.visible
        self.add_playlist_button.activate(); assert self.visible == [self.current]
        self.add_playlist_button.activate(); assert len(self.playlists['Road trip']) == 1
        self.remove_playlist_button.activate(); assert not self.visible
        count = len(self.playlists)
        self.playlist_name.text = 'road TRIP'; self.create_playlist_button.activate()
        assert len(self.playlists) == count


def main():
    with ExitStack() as assets, cui.App() as app:
        directory = Path(__file__).resolve().parents[1] / 'assets' / 'icons'
        icons = {name: assets.enter_context(cui.Icon.load(directory / (name + '.cuiicon'))) for name in ('play', 'pause')}
        app.theme(cui.DARK)
        if os.environ.get('CUI_LARGE_TEXT'):
            app.text_scale(1.5)
        demo = Cadence(app, icons)
        if os.environ.get('CUI_SMOKE_TEST'):
            app.every(100, demo.smoke)
        if os.environ.get('CUI_CAPTURE'):
            def ready():
                print('READY', flush=True)
                timer.stop()
            timer = app.every(700, ready)
        app.run()


if __name__ == '__main__':
    main()
