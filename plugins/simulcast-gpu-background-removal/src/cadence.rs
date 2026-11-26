// SPDX-License-Identifier: GPL-3.0-or-later
use std::time::{Duration, Instant};

// Schedule at a fixed phase instead of measuring from the previous frame.
// Never run a burst of inference jobs to catch up after a stall.
#[derive(Default)]
pub struct Cadence {
    next: Option<Instant>,
    rate: u32,
}
impl Cadence {
    pub fn reset(&mut self) { self.next = None; }
    pub fn due(&mut self, now: Instant, rate: u32) -> bool {
        if self.rate != rate {
            self.rate = rate;
            self.next = None;
        }
        if rate == 0 { return true; }
        let step = Duration::from_secs_f64(1.0 / f64::from(rate));
        let Some(next) = self.next else {
            self.next = Some(now + step);
            return true;
        };
        // Compensate for sub-millisecond render jitter at matched FPS.
        if now + Duration::from_millis(1) < next { return false; }
        let future = next + step;
        self.next = Some(if future <= now { now + step } else { future });
        true
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn thirty_on_sixty() {
        let t = Instant::now();
        let mut c = Cadence::default();
        let count = (0..60).filter(|i| c.due(t + Duration::from_secs_f64(*i as f64 / 60.),30)).count();
        assert_eq!(count,30);
    }
    #[test] fn no_catchup_and_rate_changes() {
        let t=Instant::now();
        let mut c=Cadence::default();
        assert!(c.due(t,15));
        assert!(!c.due(t+Duration::from_millis(20),15));
        let later=t+Duration::from_secs(2);
        assert!(c.due(later,15));
        assert!(!c.due(later,15));
        assert!(c.due(later,30));
        assert!(c.due(later,0));
        assert!(c.due(later,0));
        c.reset();
        assert!(c.due(later,20));
    }
}
